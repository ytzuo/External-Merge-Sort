#pragma once
#include "../include/MergeSortFile.h"
#include <cstddef>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

class OutputBuffer {
private:
    std::vector<int> buffer;
    size_t buffer_size;
    MergeSortFile file;

public:
    OutputBuffer(std::string filename, size_t buffer_size): 
        buffer_size(buffer_size), 
        file(filename) {
        this->file.open();
        this->buffer.resize(buffer_size);
    }

    ~OutputBuffer() {
        if(file.is_open()) {
            file.close();
        }
    }

    /* 将当前Buffer内的数据作为新的 run 写入文件末尾 */
    bool wrtiteSegToDisk();

    /* 将当前Buffer内的数据加入文件最后一个 run 末尾 */
    bool appendSegToDisk();

    /* 将胜出的元素加入Buffer */
    void pushToBuffer(int num);
};