// TODO : 实现k路归并中, 新增的三个线程

// TODO : 实现归并线程: 需要有k个叶子的败者树或k个输入来源的最小堆
//        需要实现一个管理叶子或输入来源对应的缓冲区的队列

// TODO : 实现输入线程, 维护空缓冲区池, 
//        每次从最可能被耗尽的归并段中读取一个缓冲区的数据并加入对应队列

// TODO : 实现输出线程, 类似于threads.cpp中的实现
//        管理两个输出缓冲区

#include <thread>
#include "buffer_manager.h"
#include "k_way_merge_threads.h"


MergeThread::
MergeThread(int K,
            std::vector<BufferQueue*>& qs,
            std::vector<int64_t>& last_key,
            std::vector<OutputBuffer*>& outs,
            BufferPool* pool)
    : K(K), qs(qs), last_key(&last_key), outs(outs), pool(pool){}

void MergeThread::
kWayMerge() {

}



InputThread::
InputThread(int K,
            RunStore* store,
            BufferPool* pool,
            std::vector<BufferQueue*>& qs,
            std::vector<int64_t>& last_key,
            std::vector<int> run_nums,
            std::vector<std::vector<int>> task)
    : K(K), in_store(store), pool(pool), qs(qs),
    last_key(&last_key),
    run_nums(std::move(run_nums)), task(std::move(task)) {}

void InputThread::
inputRun() {
}
