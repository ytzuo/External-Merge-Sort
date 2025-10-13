#include "buffer.h"
#include "run_store.h"
#include <csignal>
#include <cstddef>
#include <cstdint>

/* 实现 InputBuffer */
InputBuffer::
InputBuffer(RunStore& store, uint32_t run_id, size_t buffer_size)
    : store_(store), buffer_size_(buffer_size), run_id_(run_id),
    buffer_pos_(0), buffer_end_(0), consumed_(0) {
    auto[ptr, size] = store.get_run(run_id);
    total_size_ = size;

    buffer_.reserve(buffer_size_);
    /* 加载第一块 */
    if(size > 0) {
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
    size_t size_to_read = std::min(buffer_size_, 
        static_cast<size_t>(total_size_-consumed_));
    if (size_to_read == 0) {
        buffer_end_ = 0;
        buffer_pos_ = 0;
        return;
    }

    buffer_pos_  = 0;
    auto[ptr, size] = store_.get_run_range(run_id_, consumed_, size_to_read);
    buffer_size_ = size;
    buffer_.reserve(buffer_size_);
    for (size_t i = 0; i < buffer_size_; ++i) {
        buffer_[i] = ptr[consumed_ + i];
    }
}


/* 实现 OutputBuffer */
OutputBuffer::OutputBuffer(RunStore& store, size_t buffer_size)
    : store_(store), buffer_size_(buffer_size) {
    buffer_.reserve(buffer_size_);
}

void OutputBuffer::
add(int64_t value) {
    buffer_.push_back(value);
    if(buffer_.size() >= buffer_size_) {
        flush();
    }
}

void OutputBuffer::
flush() {
    if(!buffer_.empty()) {
        store_.add_run(buffer_);
        buffer_.clear();
        buffer_.reserve(buffer_size_);
    }
}

OutputBuffer::~OutputBuffer() {
    flush();
}