#pragma once
#include "run_store.h"
#include <cstdint>
#include <vector>
#include <cstddef>

/* 输入缓冲区类，用于从RunStore读取数据 */
class InputBuffer {
public:
    InputBuffer(RunStore& store, uint32_t run_id);
    
    /* 检查是否还有数据可读 */
    bool has_next() const;
    
    /* 读取下一个元素 */
    int64_t next();
    
    /* 预读下一个元素但不移动指针 */
    int64_t peek() const;
    
private:
    const int64_t* data_;
    uint64_t size_;
    uint64_t pos_;
};

/* 输出缓冲区类，用于向RunStore写入数据 */
class OutputBuffer {
public:
    explicit OutputBuffer(RunStore& store, size_t buffer_size = 1 << 20);
    
    /* 添加一个元素到缓冲区 */
    void add(int64_t value);
    
    /* 刷新缓冲区，将剩余数据写入存储 */
    void flush();
    
    ~OutputBuffer();
    
private:
    RunStore& store_;
    std::vector<int64_t> buffer_;
    size_t buffer_size_;
};