#include "../include/InputBuffer.h"
#include <cstddef>
#include <fstream>
#include <optional>
#include <vector>

/* 从磁盘读取数据填充Buffer */
bool InputBuffer::
fillFromDisk(int segId, size_t startOffset) {
    if(!file.is_open()) return false;
    file.readSegmentChunk(segId, startOffset, buffer_size, buffer);
    return true;
}

/* 首次生成归并段时用于获取单条记录 */
int InputBuffer::
getFromBuffer() {
    return buffer.at(index++);
}