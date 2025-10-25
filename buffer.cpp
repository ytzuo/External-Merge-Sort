#include "buffer.h"
#include "run_store.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>

/* 实现 InputBuffer */
InputBuffer::
InputBuffer(RunStore& store, uint32_t run_id, size_t buffer_size)
    : store_(store), buffer_size_(buffer_size), run_id_(run_id),
    buffer_pos_(0), buffer_end_(0), consumed_(0) {
    // 直接从文件元数据中获取run的实际元素数量
    try {
        total_size_ = store.get_run_size(run_id);
    } catch (...) {
        total_size_ = 0;
    }
    
    //std::cout << "创建InputBuffer: run_id=" << run_id << ", total_size=" << total_size_ << std::endl;

    buffer_.reserve(buffer_size_);
    /* 加载第一块 */
    if(total_size_ > 0) {
        load_next_block();
    }
}

bool InputBuffer:: 
has_next() const{
    return consumed_ < total_size_;
}

int64_t InputBuffer::
next() {
    if(!has_next()) return -1;

    if(buffer_pos_ >= buffer_end_) {
        load_next_block();
    }
    int64_t value =  buffer_[buffer_pos_++];
    consumed_++;
    return value;
}

int64_t InputBuffer::
peek() {
    if(!has_next()) return -1;

    if(buffer_pos_ >= buffer_end_) {
        load_next_block();
    }
    return buffer_[buffer_pos_];
}

void InputBuffer::
load_next_block() {
    // 检查是否还有数据需要读取
    if (consumed_ >= total_size_) {
        buffer_end_ = 0;
        buffer_pos_ = 0;
        return;
    }
    
    // 计算还需要读取多少数据
    size_t remaining = static_cast<size_t>(total_size_ - consumed_);
    size_t size_to_read = std::min(buffer_size_, remaining);
    
    //std::cout << "加载数据块: run_id=" << run_id_ << ", consumed=" << consumed_ 
              //<< ", remaining=" << remaining << ", size_to_read=" << size_to_read << std::endl;
    
    if (size_to_read == 0) {
        buffer_end_ = 0;
        buffer_pos_ = 0;
        return;
    }

    buffer_pos_  = 0;
    // 使用拥有映射，确保数据在复制期间有效
    MappedRange m = store_.map_run_range_owned(run_id_, consumed_, size_to_read);
    const int64_t* ptr = reinterpret_cast<const int64_t*>(m.data);
    buffer_end_ = m.bytes / sizeof(int64_t);
    buffer_.resize(buffer_end_);
    for (size_t i = 0; i < buffer_end_; ++i) {
        buffer_[i] = ptr[i];
    }
    
    //std::cout << "数据块加载完成: run_id=" << run_id_ << ", elements=" << size << std::endl;
}

// 设置 active 状态, 当明确知道要设置为什么状态时使用
void InputBuffer::
set_active(bool value) {
    active.store(value);
}

// 获取 active 状态
bool InputBuffer::
is_active() const {
    return active.load();
}

// 原子的反转 active 状态
bool InputBuffer::
toggle_active() {
    return active.exchange(!active.load());
}

void InputBuffer::
resetBuffer(uint32_t run_id) {
    this->run_id_ = run_id; // 重新设置读取的 run_id
    try {
        total_size_ = this->store_.get_run_size(run_id);
    } catch (...) {
        total_size_ = 0;
    }

    buffer_pos_ = 0;
    buffer_end_ = 0;
    consumed_   = 0;
    buffer_.reserve(buffer_size_);
    /* 加载第一块 */
    if(total_size_ > 0) {
        load_next_block();
    }
}

void InputBuffer::load_chunk(uint32_t run_id, uint64_t offset, uint64_t count) {
    // 重新设置当前buffer关联的run和相关参数
    this->run_id_ = run_id;
    
    // 使用拥有映射，确保数据在复制期间有效
    MappedRange m = store_.map_run_range_owned(run_id, offset, count);
    const int64_t* ptr = reinterpret_cast<const int64_t*>(m.data);
    
    // 调整buffer大小并复制数据
    buffer_.resize(count);
    for (size_t i = 0; i < count; ++i) {
        buffer_[i] = ptr[i];
    }
    
    // 重置缓冲区状态
    buffer_pos_ = 0;
    buffer_end_ = count;
    total_size_ = count;
    consumed_ = 0;
}









/* 实现 OutputBuffer */
OutputBuffer::OutputBuffer(RunStore& store, size_t buffer_size)
    : store_(store), buffer_size_(buffer_size) {
    buffer_.reserve(buffer_size_);
    //std::cout << "创建OutputBuffer: buffer_size=" << buffer_size << std::endl;
}

void OutputBuffer::
add(int64_t value) {
    // 还没有一个新的 run 就创建一个
    if(!run_started_) {
        //std::cout<<"begin_run..."<<std::endl;
        store_.begin_run();
        run_started_ = true;
    }

    buffer_.push_back(value);

    // 缓冲区已满, 写入磁盘
    if(buffer_.size() >= buffer_size_) {
        store_.append_to_run(buffer_);
        buffer_.clear();
        buffer_.reserve(buffer_size_);
    }
}

void OutputBuffer::
flush() {
    // 写入磁盘
    if(!buffer_.empty()) {
        store_.append_to_run(buffer_);
        buffer_.clear();
        buffer_.reserve(buffer_size_);
    }
    // 结束 run
    if(run_started_) {
        //std::cout<<"end_run..."<<std::endl;
        store_.end_run();
        run_started_ = false;
    }
}

void OutputBuffer::
flush_direct() {
    if(!buffer_.empty()) {
        store_.append_direct(buffer_);
        buffer_.clear();
        buffer_.reserve(buffer_size_);
    }
}

bool OutputBuffer::
empty() {
    return buffer_.empty();
}

OutputBuffer::~OutputBuffer() {
    flush();
}

// 设置 active 状态, 当明确知道要设置为什么状态时使用
void OutputBuffer::
set_active(bool value) {
    active.store(value);
}

// 获取 active 状态
bool OutputBuffer::
is_active() const {
    return active.load();
}

// 原子的反转 active 状态
bool OutputBuffer::
toggle_active() {
    return active.exchange(!active.load());
}