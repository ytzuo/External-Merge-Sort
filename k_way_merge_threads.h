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
    std::vector<BufferQueue*> qs;             // 非拥有
    std::vector<int64_t>* last_key;           // 非拥有
    std::vector<int> run_nums;
    std::vector<std::vector<int>> task;
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

struct Orchestrator {
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