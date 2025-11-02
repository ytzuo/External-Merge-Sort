// TODO : 实现k路归并中, 新增的三个线程

// TODO : 实现归并线程: 需要有k个叶子的败者树或k个输入来源的最小堆
//        需要实现一个管理叶子或输入来源对应的缓冲区的队列

// TODO : 实现输入线程, 维护空缓冲区池, 
//        每次从最可能被耗尽的归并段中读取一个缓冲区的数据并加入对应队列

// TODO : 实现输出线程, 类似于threads.cpp中的实现
//        管理两个输出缓冲区

#include <thread>
#include <algorithm>
#include "buffer_manager.h"
#include "k_way_merge_threads.h"

/* 规划归并任务 */
std::vector<std::vector<int>> generate_task(int K, int total_runs) {
    std::vector<std::vector<int>> tasks;
    int next_turn_runs = total_runs;
    int turn_beg = 0;
    while(next_turn_runs != 1) {
        std::vector<int> t;
        int end = std::min(turn_beg + K, turn_beg + next_turn_runs);
        // std::cout<<end<<std::endl;
        for(int i = turn_beg; i < end; i++) {
            t.push_back(i);
        }
        tasks.push_back(t);
        turn_beg += K;
        next_turn_runs -= std::min(K, next_turn_runs);
        next_turn_runs ++;
    }
    return tasks;
}

MergeThread::
MergeThread(int K,
            std::vector<BufferQueue*>& qs,
            std::vector<int64_t>& last_key,
            std::vector<OutputBuffer*>& outs,
            BufferPool* pool)
    : K(K), qs(qs), last_key(&last_key), outs(outs), pool(pool){}

void MergeThread::
kWayMerge() {
    /*
        从k个输入来源中读取数据, 直到所有输入来源都为空
        当某个输入来源队列中没有数据了, 要判断是否是本段已经全部处理完毕, 若不是, 等待新的有数据的缓冲区进入队列
        使用败者树或最小堆来管理k个输入来源, 要记录下每个数据对应的输入来源
        当k个输入来源都为空, 进入下一轮
        当输出缓冲区满时, 切换空闲的输出缓冲区
    */
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
    /*
        只要还有空闲缓冲区, 尝试根据 last_key 确定一个未读取完毕的段
        从段中读取一个缓冲区的数据并加入对应的缓冲区队列
        当所有段都读取完毕时, 等待归并线程发出本轮结束的信号, 开始新一轮的读取
    */
}
