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
        bufferPool = std::vector<InputBuffer*>(0);
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
    bool hasNext = true;
    uint64_t run_size = 0;        // 整个 run 的大小
    uint64_t elements_read = 0;   // 已读取的元素数

public:
    size_t getQueueSize() {
        return bufferQueue.size();
    }

    void setRunSize(uint64_t size) {
        run_size = size;
        elements_read = 0;
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
        elements_read += buffer->get_chunk_size();  // 累加已读取的元素
        if(elements_read == run_size)
            hasNext = false;
        // std::cout<<"缓冲区进入队列, totalNum = "<<totalNum
        //          <<", chunk_size = "<<buffer->get_chunk_size()
        //          <<", elements_read = "<<elements_read
        //          <<", run_size = "<<run_size
        //          <<", hasNext = " << hasNext << std::endl;
        //hasNext = buffer->has_next();
    }

    bool has_next() {
        return elements_read < run_size; 
    }

    // 从队列中获取一个缓冲区
    InputBuffer* getBuffer() {
        if (bufferQueue.empty()) {
            return nullptr;
        }
        InputBuffer* buffer = bufferQueue.front();
        bufferQueue.pop();
        totalNum--;
        return buffer;
    }
};