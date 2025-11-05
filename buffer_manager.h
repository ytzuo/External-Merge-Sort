#pragma once
#include <cstddef>
#include <vector>
#include <queue>
#include "buffer.h"
// 缓冲区队列管理和空闲缓冲区池

class BufferPool {
private:
    std::vector<InputBuffer*> bufferPool;
    size_t poolSize;

public:
    // BufferPool(std::vector<InputBuffer*> externalBufferPool) {
    //     // 直接使用外部传递的缓冲区池
    //     bufferPool = externalBufferPool;
    // }

    BufferPool(size_t poolSize) {
        this->poolSize = poolSize;
        bufferPool = std::vector<InputBuffer*>(poolSize);
    }

    ~BufferPool() {
        // 清理缓冲区池
        for (auto buffer : bufferPool) {
            delete buffer;  // 销毁缓冲区
        }
    }

    size_t getPoolSize() const {
        return bufferPool.size();
    }

    // 获取一个缓冲区
    InputBuffer* getBuffer() {
        if (bufferPool.empty()) {
            return nullptr;  // 如果池为空，返回 nullptr
        }
        InputBuffer* buffer = bufferPool.back();
        bufferPool.pop_back();  // 从池中移除缓冲区
        return buffer;
    }

    // 将缓冲区归还到池中
    void returnBuffer(InputBuffer* buffer) {
        bufferPool.push_back(buffer);  // 将缓冲区返回到池中
    }
};

class BufferQueue {
private:
    std::queue<InputBuffer*> bufferQueue;
    size_t totalNum = 0;

public:
    size_t getQueueSize() {
        return bufferQueue.size();
    }

    size_t getTotalNum() {
        return totalNum;
    }

    bool empty() {
        return bufferQueue.size() == 0;
    }

    // 添加一个缓冲区进入队列
    void addBuffer(InputBuffer* buffer) {
        bufferQueue.push(buffer);
        totalNum++;
    }

    // 从队列中获取一个缓冲区
    InputBuffer* getBuffer() {
        if (bufferQueue.empty()) {
            return nullptr;
        }
        InputBuffer* buffer = bufferQueue.front();
        bufferQueue.pop();
        return buffer;
    }
};