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
    bool finish = false;

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

    bool empty();

    size_t bufferSize();

    bool is_finish() {
        return finish;
    }
};


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

    bool full();
};