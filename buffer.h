#pragma once
#include "run_store.h"
#include <atomic>
#include <cstdint>
#include <vector>
#include <cstddef>

/* 输入缓冲区类，用于从RunStore读取数据 */
class InputBuffer {
public:
    InputBuffer(RunStore& store, uint32_t run_id, size_t buffer_size = 1 << 20);
    
    // 删除拷贝构造函数和拷贝赋值运算符
    InputBuffer(const InputBuffer&) = delete;
    InputBuffer& operator=(const InputBuffer&) = delete;
    
    // 添加移动构造函数和移动赋值运算符
    InputBuffer(InputBuffer&& other) noexcept;
    InputBuffer& operator=(InputBuffer&& other) noexcept;
    
    /* 检查是否还有数据可读 */
    bool has_next() const;

    /* 缓冲区内是否还有数据 */
    bool empty() const;
    
    /* 读取下一个元素 */
    int64_t next();
    
    /* 预读下一个元素但不移动指针 */
    int64_t peek();

    /* active 标志位操作 */
    void set_active(bool value);
    bool is_active() const;
    bool toggle_active();

    /* 重置缓冲区 */
    void resetBuffer(uint32_t run_id);
    
    /* 从磁盘加载下一块数据到缓冲区 */
    void load_next_block();

    void load_chunk(uint32_t run_id, uint64_t offset, uint64_t count);
    
private:
    RunStore& store_;
    uint32_t run_id_;
    
    std::vector<int64_t> buffer_;
    size_t buffer_size_;
    size_t buffer_pos_;  // 当前在缓冲区中的位置
    size_t buffer_end_;  // 缓冲区中有效数据的结束位置
    uint64_t total_size_; // run的总大小
    uint64_t consumed_;   // 已经消费的数据数量

    std::atomic<bool> active{false};
};

/* 输出缓冲区类，用于向RunStore写入数据 */
class OutputBuffer {
public:
    explicit OutputBuffer(RunStore& store, size_t buffer_size = 1 << 20);
    
    // 删除拷贝构造函数和拷贝赋值运算符
    OutputBuffer(const OutputBuffer&) = delete;
    OutputBuffer& operator=(const OutputBuffer&) = delete;
    
    // 添加移动构造函数和移动赋值运算符
    OutputBuffer(OutputBuffer&& other) noexcept;
    OutputBuffer& operator=(OutputBuffer&& other) noexcept;
    
    /* 添加一个元素到缓冲区 */
    void add(int64_t value);
    
    /* 刷新缓冲区，将剩余数据写入存储 */
    void flush();
    void flush_direct();
    bool empty();
    ~OutputBuffer();

    /* active 标志位操作 */
    void set_active(bool value);
    bool is_active() const;
    bool toggle_active();
    
private:
    RunStore& store_;
    std::vector<int64_t> buffer_;
    size_t buffer_size_;
    bool run_started_ = false;  // 标记是否已开始run

    std::atomic<bool> active{false};
};