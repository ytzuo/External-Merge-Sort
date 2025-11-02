#pragma once
#include "buffer.h"
#include "buffer_manager.h"
#include "multi_run_file.h"
#include "run_store.h"
#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

/*
    K路归并多线程架构设计
    
    【三个线程协作模型】
    
    1. InputThread (读取线程):
       - 从 RunStore 读取数据到缓冲区
       - 将填充好的缓冲区加入对应的 BufferQueue
       - 更新 last_key 告知 MergeThread 读取进度
    
    2. MergeThread (归并线程):
       - 从 K 个 BufferQueue 获取缓冲区
       - 使用最小堆执行 K 路归并
       - 将结果写入双输出缓冲区
    
    3. WriterThread (写入线程):
       - 监控双输出缓冲区的 active 标志
       - 将满的缓冲区写入 RunStore
       - 写完后标记为空闲供 MergeThread 继续使用
    
    【核心同步机制】
    
    - BufferQueue (K个): InputThread → MergeThread 的数据通道
    - BufferPool (1个): 空闲输入缓冲区池，循环使用
    - last_key (K个): 记录每路输入的读取进度，用于判断是否读完
    - OutputBuffer (2个): 双缓冲机制，通过 is_active() 同步
    - task (vector<vector<int>>): 提前规划好的所有归并任务
    
    【数据流转过程】
    
    InputThread:  pool → load_chunk → qs[i] → (更新last_key)
                   ↑                              ↓
                   |                          MergeThread: qs[i] → merge → outs[0/1] → set_active
                   |                              ↓                          ↓
                   └──── returnBuffer ← 用完 ←──┘                      WriterThread: flush → set_inactive
    
    【避免死锁的关键】
    
    - 所有等待都使用 yield() 而非阻塞锁
    - 缓冲区用完立即归还，不持有
    - 双缓冲确保写入和归并可并行
    - last_key 作为完成判断依据，避免无限等待
*/

// MergeThread：只拿共享通道的引用/指针，不持有 InputThread 指针
class MergeThread {
private:
    int K;
    std::vector<BufferQueue*> qs;            // 各段输入队列（非拥有）
    std::vector<int64_t>* last_key;          // 共享调度视图（非拥有）
    std::vector<OutputBuffer*> outs;         // 双输出缓冲（非拥有）
    BufferPool* pool;                         // 用于回收输入缓冲（非拥有）

    // 每轮开始时初始化 K 个输入缓冲区
    void initializeRound(const std::vector<int>& run_ids);
public:
    MergeThread(int K,
                std::vector<BufferQueue*>& qs,
                std::vector<int64_t>& last_key,
                std::vector<OutputBuffer*>& outs,
                BufferPool* pool);

    void kWayMerge(std::vector<OutputBuffer>& outputs);
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