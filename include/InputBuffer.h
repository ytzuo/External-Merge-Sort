#pragma once
#include <cstddef>
#include <fstream>
#include <optional>

template<typename T>
class InputBuffer {
private:
    T* begin_;
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
        this->current_index_ = 1; // 注意这个current_index_是从1开始
        this->file_ = std::fstream(file_path, std::ios::binary | std::ios::in);
        if (!this->file_.is_open()) {
            return false;
        }
        // 为缓冲区分配内存
        if (begin_) {
            delete[] begin_; // 如果已有内存，先释放
        }
        
        begin_ = new T[block_size];
        cap_ = begin_ + block_size;
        end_ = begin_;
        cur_ = begin_;
        
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
    if(!isRunEndValid() && !isRunPosValid()) {

    }
}