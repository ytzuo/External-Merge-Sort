#include "buffer.h"
#include "run_store.h"
#include <cstddef>
#include <cstdint>

/* 实现 InputBuffer */
InputBuffer::
InputBuffer(RunStore& store, uint32_t run_id) {
    auto[ptr, size] = store.get_run(run_id);
    this->data_ = ptr;
    this->size_ = size;
    this->pos_  = 0; 
}

bool InputBuffer::
has_next() const{
    return pos_ < size_;
}

int64_t InputBuffer::
next() {
    return data_[pos_++];
}

int64_t InputBuffer::
peek() const {
    return data_[pos_];
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