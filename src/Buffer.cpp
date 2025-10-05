#include "../include/Buffer.h"
#include <cstddef>
#include <vector>
//#include <iostream>

/* 从磁盘读取数据填充Buffer */
bool InputBuffer::
fillFromDisk(int segId, size_t startOffset) {
    if(!file.is_open()) return false;
    buffer.clear();
    bool ret =  file.readSegmentChunk(segId, startOffset, buffer_size, buffer);
    if(ret == false)
        finish = true;
    return ret;
}

/* 首次生成归并段时用于获取单条记录 */
int InputBuffer::
getFromBuffer() {
    return buffer.at(index++);
}

bool InputBuffer::
empty() {
    //std::cout<<"IN empty() "<<index<<std::endl;
    return index == buffer_size;
}

size_t InputBuffer::
bufferSize() {
    return buffer_size;
}


bool OutputBuffer::
wrtiteSegToDisk() {
    try {
        if(!file.is_open()) return false;
        file.appendSegment(buffer);
        buffer.clear();
        return true;
    } catch(...) {
        return false;
    }
}

bool OutputBuffer::
appendSegToDisk() {
    try {
        if(!file.is_open()) return false;
        file.append(buffer);
        buffer.clear();
        return true;
    } catch(...) {
        return false;
    }
}

void OutputBuffer::
createNewSeg() {
    // 首先将当前缓冲区的数据写入磁盘（如果有的话）
    if (!empty()) {
        wrtiteSegToDisk();
    }
    // 总是创建一个新段，而不是检查文件是否为空
    std::vector<int> emptyRun;
    file.appendSegment(emptyRun);
}

void OutputBuffer::
pushToBuffer(int num) {
    buffer.push_back(num);
}

bool OutputBuffer::
full() {
    //std::cout<<"IN full() "<<buffer.size()<<std::endl;
    return buffer.size() == buffer_size;
}

bool OutputBuffer::
empty() {
    return buffer.size() == 0;
}

int OutputBuffer::
size() {
    return buffer.size();
}

void OutputBuffer::
clear() {
    buffer.clear();
}