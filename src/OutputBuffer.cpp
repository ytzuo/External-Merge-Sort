#include "../include/OutputBuffer.h"

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
pushToBuffer(int num) {
    buffer.push_back(num);
}