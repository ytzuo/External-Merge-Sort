#pragma once
#include <cstddef>
#include <fstream>

template<typename T>
class InputBuffer {
private:
    T* begin_;
    T* end_;
    T* cap_;

    std::ifstream* file_;   // 关联的输入文件流指针
    size_t block_size_;     // 块大小（元素个数）
    size_t current_pos_;    // 当前读取位置

public:
    InputBuffer();
    ~InputBuffer();

    bool init(std::ifstream file, size_t block_size, size_t current_pos);
    bool scan();
};