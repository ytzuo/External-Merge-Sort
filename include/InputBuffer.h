#pragma once
#include "../include/MergeSortFile.h"
#include <cstddef>
#include <fstream>
#include <optional>
#include <vector>

class InputBuffer {
private:
    std::vector<int> buffer;
    size_t buffer_size;
    size_t index;
    //std::string filename;
    MergeSortFile file;

public:
    InputBuffer(std::string filename, size_t buffer_size): 
        buffer_size(buffer_size), 
        index(0), 
        file(filename) {
        this->file.open();
        this->buffer.resize(buffer_size);
    }

    ~InputBuffer() {
        if(file.is_open()) {
            file.close();
        }
    }

    /* 从磁盘读取数据填充Buffer */
    bool fillFromDisk(int segId, size_t startOffset);

    /* 首次生成归并段时用于获取单条记录 */
    int  getFromBuffer();
};