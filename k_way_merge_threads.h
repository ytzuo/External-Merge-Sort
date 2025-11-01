#pragma once
#include "buffer_manager.h"
#include <vector>

class mergeThread {
private:
    int K; // K 路
    std::vector<BufferQueue*> bufferQueue; // 储存K个缓冲区队列
    std::vector<int64_t> last_key; // 维护每个段的最后读的一个key, 用于确定下一个读的段
public:
    mergeThread(int K) {
        this->K = K;
        for (int i = 0; i < K; i++) {
            bufferQueue.push_back(new BufferQueue());
            last_key.push_back(0);
        }
    }
    void kWayMerge() {

    }
};

class inputThread {
private:
    int K; // K 路
    BufferPool* bufferPool; // 储存缓冲区的池子
    mergeThread* worker;        // 需要一个指向 mergeThread 的指针，以便查询
    std::vector<int> run_nums; // 维护每个缓冲区队列对应的段的编号
    std::vector<std::vector<int>> task; // 维护每个缓冲区队列负责的段的列表
public:
    inputThread(int K, mergeThread* worker, std::vector<int> run_nums, std::vector<std::vector<int>> task) {
        this->K = K;
        this->worker = worker;
        this->run_nums = run_nums;
        this->task = task;
        bufferPool = new BufferPool(K);
    }

    void initPool() {
        // 创建K个缓冲区队列中的初始缓冲区, 并读取数据
        for(int i = 0; i < K; i++) {
            
        }
    }

    void inputRun() {

    }
};

/* 这个好像不需要, 可以复用threads.cpp中的writer */
// class outputThread {
// private:


// public:
// };