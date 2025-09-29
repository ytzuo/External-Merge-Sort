#pragma once
#include "FileHelper.h"
#include <cstddef>
#include <fstream>
#include <optional>

template<typename T>
class InputBuffer {
private:
    T* beg_;
    T* end_;
    T* cur_;
    T* cap_;

    std::fstream file_;    // 关联的输入文件流指针
    size_t block_size_;     // 块大小（元素个数）
    int    current_index_;  // 当前读取的run的index

    // 使用std::optional表示可能未初始化的状态
    std::optional<std::streampos> run_pos_; // 当前读取的run的位置对应的文件指针
    std::optional<std::streampos> run_end_; // 当前读取的run的结尾对应的文件指针

    // 检查指针是否初始化的方法
    bool isRunPosValid() const { return run_pos_.has_value(); }
    bool isRunEndValid() const { return run_end_.has_value(); }

    FileHelper helper;

public:
    InputBuffer();
    ~InputBuffer();

    bool init(std::string file_path, size_t block_size);
    bool scan();
};

template<typename T>
bool InputBuffer<T>::
init(std::string file_path, size_t block_size) {
    try {
        this->block_size_ = block_size;
        this->current_index_ = 0; // 注意这个current_index_是从1开始
        this->file_ = std::fstream(file_path, std::ios::binary | std::ios::in);
        if (!this->file_.is_open()) {
            return false;
        }
        // 为缓冲区分配内存
        if (beg_) {
            delete[] beg_; // 如果已有内存，先释放
        }
        
        beg_ = new T[block_size];
        cap_ = beg_ + block_size;
        end_ = beg_;
        cur_ = beg_;
        
        // 初始化run指针为未设置状态
        run_pos_ = std::nullopt;
        run_end_ = std::nullopt;

    } catch(...) {
        return false;
    }
    return true;
}

template<typename T>
bool InputBuffer<T>::
scan() {
    if(!isRunEndValid() && !isRunPosValid() || // 两个文件指针都无效
        run_pos_ != run_end_) { // 读取到一个run的结尾
        // 获取对应或下一条run的指针
        this->current_index_ ++;
        auto [pos, end] = helper.getRunInfo<T>(this->file_, this->current_index_);
        this->run_pos_ = pos;
        this->run_end_ = end;
    }
    helper.scan(file_, this->run_pos_, this->run_end_, 
        this->block_size_, this->beg_, this->end_);
}