#pragma once
#include "buffer.h"
#include "buffer_manager.h"
#include "multi_run_file.h"
#include "run_store.h"
#include <vector>
#include <thread>

// MergeThread：只拿共享通道的引用/指针，不持有 InputThread 指针
class MergeThread {
private:
    int K;
    std::vector<BufferQueue*> qs;            // 各段输入队列（非拥有）
    std::vector<int64_t>* last_key;          // 共享调度视图（非拥有）
    std::vector<OutputBuffer*> outs;         // 双输出缓冲（非拥有）
    BufferPool* pool;                         // 用于回收输入缓冲（非拥有）
public:
    MergeThread(int K,
                std::vector<BufferQueue*>& qs,
                std::vector<int64_t>& last_key,
                std::vector<OutputBuffer*>& outs,
                BufferPool* pool);

    void kWayMerge();
};

// InputThread：不拿 MergeThread 指针；直接读 last_key 和队列
class InputThread {
private:
    int K;
    RunStore* in_store;                       // 非拥有
    BufferPool* pool;                         // 非拥有
    std::vector<BufferQueue*> qs;             // 非拥有 用于将读取完毕的缓冲区加入队列
    std::vector<int64_t>* last_key;           // 非拥有
    std::vector<int> run_nums;
    std::vector<std::vector<int>> task;
    /*
        task说明: 
            假设有10个归并段和4路归并, 那么task就包含三个vector<int>, 分别为:
                [0, 1, 2, 3], [4, 5, 6, 7], [8, 9, 10, 11]
            第三个task中的10和11来自于前两次任务生成的
                
            当只剩下一个vector<int>, 且只包含一个段时, 说明归并已经完成

        对于k路归并的过程:
            每次处理k个归并段, 直到所有归并段处理完毕
            首先在k个队列中分别读取数据, 接着不断补充, 直到所有队列中都不含有数据后, 本轮结束
            重复上述过程, 直到所有任务处理完成
    */
public:
    InputThread(int K,
                RunStore* store,
                BufferPool* pool,
                std::vector<BufferQueue*>& qs,
                std::vector<int64_t>& last_key,
                std::vector<int> run_nums,
                std::vector<std::vector<int>> task);

    void inputRun();
};

class kWayMergeManager {
private:
    // 拥有的资源
    int K;
    RunStore* store;                                        // 非拥有（由上层创建/销毁）
    std::vector<std::unique_ptr<BufferQueue>> q_storage;    // 真正持有
    std::vector<BufferQueue*> qs;                           // 裸指针视图（便于传参）
    std::vector<int64_t> last_key;                          // 共享调度视图
    std::vector<OutputBuffer*> outs;                        // 由上层传入管理（非拥有）
    BufferPool pool;                                        // 空闲输入缓冲池（你已改为外部注入构造）

    // 线程实体
    MergeThread merge;
    InputThread input;

    // 线程对象
    std::thread t_input;
    std::thread t_merge;
    std::thread t_writer;
};

std::vector<std::vector<int>> generate_task(int K, int total_runs);