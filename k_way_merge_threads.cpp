// TODO : 实现k路归并中, 新增的三个线程

// TODO : 实现归并线程: 需要有k个叶子的败者树或k个输入来源的最小堆
//        需要实现一个管理叶子或输入来源对应的缓冲区的队列

// TODO : 实现输入线程, 维护空缓冲区池, 
//        每次从最可能被耗尽的归并段中读取一个缓冲区的数据并加入对应队列

// TODO : 实现输出线程, 类似于threads.cpp中的实现
//        管理两个输出缓冲区

#include <thread>
#include <algorithm>
#include <queue>
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
initializeRound(const std::vector<int>& run_ids) {
    /* 
        一轮归并完成后应该是没有非空输入缓冲区的
        此时从 pool 中取出 K 个缓冲区并初始化新的 run_id
     */
     for(int i = 0; i < K; i++) {
        InputBuffer* buffer = pool->getBuffer();
        if(buffer == nullptr) {
            std::cout << "MergeThread: pool is empty" << std::endl;
            std::this_thread::yield();
        }
        buffer->resetBuffer(run_ids[i]);
        qs[run_ids[i]]->addBuffer(buffer);
    }
}

void MergeThread::
kWayMerge(std::vector<OutputBuffer>& outputs) {

    // 初始化最小堆
    std::priority_queue<std::pair<int64_t, int>, 
                        std::vector<std::pair<int64_t, int>>, 
                        std::greater<std::pair<int64_t, int>>> pq;
    std::vector<InputBuffer*> current_buffers(K, nullptr); // 当前正在使用的缓冲区
    int cur_out = 0;
    uint64_t cur_out_count = 0;
    /*
        【归并线程同步逻辑设计】
        
        核心职责：从k个输入队列中取出缓冲区，执行K路归并，输出到双缓冲区
        
        1. 数据结构准备：
           - 使用最小堆管理K路输入：priority_queue<pair<int64_t, int>, ..., greater>
             - pair.first: 当前值
             - pair.second: 来源索引(0~K-1)
           - current_buffers[K]: 记录每个源当前正在使用的缓冲区指针
           - cur_out: 当前使用的输出缓冲区索引(0或1)
           - cur_out_count: 当前输出缓冲区已写入的元素数量
        
        2. 初始化阶段（每轮任务开始）：
           - 遍历 task 中的每个归并任务 vector<int>
           - 对于每个任务的K个run_id：
             a) 从对应的 qs[i] 队列获取第一个缓冲区
             b) 如果队列为空，spin等待 InputThread 填充数据
             c) 将每个缓冲区的第一个元素加入最小堆
        
        3. 归并循环：
           while (堆非空 || 还有未读完的缓冲区):
             a) 从堆顶取出最小元素 (val, src_idx)
             b) 将 val 写入 outs[cur_out]
             c) 从 current_buffers[src_idx] 读取下一个元素：
                - 如果缓冲区还有数据：继续加入堆
                - 如果缓冲区已空：
                  * 将空缓冲区归还到 pool (pool->returnBuffer)
                  * 从 qs[src_idx] 获取下一个缓冲区
                  * 如果队列为空且 last_key[src_idx] 未达到run末尾：
                    spin等待 InputThread 补充
                  * 如果该run已读完：标记该源结束，堆中不再加入此源元素
             d) 检查输出缓冲区：
                - 如果 cur_out_count >= 阈值(如64K)：
                  * 等待 outs[cur_out].is_active() == false (Writer未占用)
                  * outs[cur_out].set_active(true) 通知Writer写入
                  * 切换到另一个输出缓冲区: cur_out = 1 - cur_out
                  * 重置计数: cur_out_count = 0
        
        4. 任务结束阶段：
           - 将最后一个输出缓冲区标记为active，通知Writer
           - 重复步骤2，处理下一个归并任务，直到所有任务完成
        
        5. 线程同步关键点：
           - 与InputThread同步：通过 qs[i] 队列 + last_key[i] 判断是否还有数据
           - 与WriterThread同步：通过 outs[i].is_active() 双缓冲机制
           - 缓冲区回收：用完立即归还 pool，供InputThread复用
           
        注意事项：
           - 避免死锁：获取缓冲区时使用 yield() 而非阻塞等待
           - last_key[i] 由InputThread维护，表示第i路已读到的最后一个key位置
           - 当所有task完成后，设置全局标志通知其他线程结束
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
        【输入线程同步逻辑设计】
        
        核心职责：从RunStore读取数据到缓冲区，按需补充K个队列
        
        1. 数据结构准备：
           - run_offsets[K]: 记录每个输入源当前读取到的偏移量
           - run_sizes[K]: 记录每个输入源的总大小（元素数）
           - current_task_index: 当前正在处理的任务索引
           - current_run_ids[K]: 当前任务对应的K个run_id
        
        2. 任务初始化（每轮任务开始）：
           - 从 task[current_task_index] 获取当前要处理的run列表
           - 初始化 run_offsets 全部为0
           - 查询每个run的大小，填充 run_sizes
           - 更新 last_key 为每个run的初始位置（或特殊值表示未开始）
        
        3. 主循环（为当前任务补充数据）：
           while (当前任务的某些run还未读完):
             a) 从 pool 获取空闲缓冲区:
                - buffer = pool->getBuffer()
                - 如果 buffer == nullptr: yield() 等待MergeThread归还缓冲区
             
             b) 选择要读取的run（调度策略）:
                策略1：轮询法 - 依次为每个run读取
                策略2：优先级法 - 根据 last_key 判断哪个队列最可能被耗尽
                推荐：简单轮询，找到第一个 run_offsets[i] < run_sizes[i] 的源
             
             c) 读取数据到缓冲区:
                - 确定读取的run_id和offset
                - buffer->load_chunk(run_id, offset, count)
                - 更新 run_offsets[i] += count
                - 更新 last_key[i] 为当前读取的最后一个key值（用于MergeThread判断）
             
             d) 将缓冲区加入对应队列:
                - qs[i]->addBuffer(buffer)
             
             e) 检查当前任务是否完成:
                - 如果所有 run_offsets[i] >= run_sizes[i]:
                  * 进入下一个任务: current_task_index++
                  * 如果 current_task_index >= task.size(): 退出线程
                  * 否则重复步骤2，初始化下一轮任务
        
        4. 线程同步关键点：
           - 与MergeThread同步：
             * 通过 qs[i] 队列传递缓冲区
             * 通过 last_key[i] 告知MergeThread当前进度
           - 缓冲区管理：
             * 从 pool 获取空闲缓冲区
             * MergeThread用完后归还到pool
             * 使用 yield() 等待而非阻塞
        
        5. 调度优化建议：
           - 可以根据 qs[i].getQueueSize() 判断哪个队列最需要补充
           - 保证每个队列至少有1-2个缓冲区排队，减少MergeThread等待
           - CHUNK_SIZE 通常设为 64KB / sizeof(int64_t) = 8192个元素
        
        注意事项：
           - last_key 的更新必须在 addBuffer 之后，保证可见性
           - 不要在持有缓冲区时长时间阻塞
           - 每轮任务结束后，确保所有相关队列已清空再进入下一轮
    */
}
