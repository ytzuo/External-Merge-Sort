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
            BufferPool* pool,
            std::vector<std::vector<int>> task,
            std::atomic<bool>& inited)
    : K(K), qs(qs), last_key(&last_key), outs(outs), pool(pool), task(task), inited(inited){}


void MergeThread::
kWayMerge(std::vector<OutputBuffer>& outputs, int task_num) {

   // 初始化最小堆
   std::priority_queue<std::pair<int64_t, int>, 
                       std::vector<std::pair<int64_t, int>>, 
                       std::greater<std::pair<int64_t, int>>> pq;
   std::vector<InputBuffer*> current_buffers(K, nullptr); // 当前正在使用的缓冲区
   int cur_out  = 0;
   int cur_task = 0;
   //uint64_t cur_out_count = 0;

   while(cur_task < task_num) {
      while(!inited.load()) {
         std::this_thread::yield();
      }

      int queue_num = task[cur_task].size();
      for(int i = 0; i < queue_num; i++) { // 初始化最小堆
         current_buffers[i] = qs[i]->getBuffer();
         pq.push(std::make_pair(current_buffers[i]->next(), i));
      }
      // 当所有队列都不为空: 每次从最小堆里弹出一个, 并从对应的缓冲区中读取下一个元素
      // 当一个缓冲区为空, 确认该缓冲区是否已经读完, 如果没有则等待输入线程填充数据
      // 已经读完则继续
      while(true) {
         // 检查有没有空的缓冲区
         for(int i = 0; i < queue_num; i++) {
            if(current_buffers[i] == nullptr) { // 当前没有缓冲区正在使用
               if(qs[i]->empty() && qs[i]->getTotalNum() == 0) { 
                  // 对应缓冲区队列也为空, 且也没有需要读取的数据
                  continue; // 跳过这个缓冲区
               } else if (!qs[i]->empty()) {
                  // 加载一个缓冲区
                  current_buffers[i] = qs[i]->getBuffer(); 
               } else if (qs[i]->getTotalNum() != 0){ 
                  // 还有需要读取的
                  i--; // 挂起等待
                  std::this_thread::yield();
               }
            }
         }

         while(!pq.empty()) {
            std::pair<int64_t, int> top = pq.top();
            int64_t val = top.first;
            int src = top.second;
            pq.pop();
            // 待完成: 写入输出缓冲区和输出控制逻辑

            if(current_buffers[src]->has_next()) { // 缓冲区还有数据
               // 从对应缓冲区队列加入一个数据到最小堆
               pq.push(std::make_pair(current_buffers[src]->next(), src));
            } else { // 移除缓冲区并还到 pool
               pool->returnBuffer(current_buffers[src]);
               current_buffers[src] = nullptr;
            }
         }
      }
      // 一轮结束, 回到未初始化状态
      inited.store(false);
      cur_task ++;
   }
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
        当某个队列为空时, 一定要判断这个队列是否还有未读取完毕的数据, 
        如果有, 要等待输入线程读取并将新的缓冲区加入队列
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

void InputThread::
initializeRound(const std::vector<int>& run_ids) {
    /* 
        一轮归并完成后应该是没有非空输入缓冲区的
        此时从 pool 中取出 K 个缓冲区并初始化新的 run_id
     */
   for(int i = 0; i < K; i++) {
      InputBuffer* buffer = pool->getBuffer();
      while(buffer == nullptr) { // 没有空闲的缓冲区, 等待
         std::this_thread::yield();
         buffer = pool->getBuffer();
      }
      
      // 获取这个段的大小
      uint64_t run_size = in_store->get_run_size(run_ids[i]);
      // 计算第一块要读取的大小
      uint64_t chunk_size = std::min(run_size, static_cast<uint64_t>(1 << 13));
      // 读取第一块数据
      if(chunk_size > 0) {
         buffer->load_chunk(run_ids[i], 0, chunk_size);
      }
      
      // 加入队列
      qs[i]->addBuffer(buffer);
   }
}

InputThread::
InputThread(int K,
            RunStore* store,
            BufferPool* pool,
            std::vector<BufferQueue*>& qs,
            std::vector<int64_t>& last_key,
            std::vector<int> run_nums,
            std::vector<std::vector<int>> task,
            std::atomic<bool>& inited)
    : K(K), in_store(store), pool(pool), qs(qs),
    last_key(&last_key),
    inited(inited),
    run_nums(std::move(run_nums)), task(std::move(task)) {}

void InputThread::
inputRun() {
   int current_task_index = 0;
   while (current_task_index < task.size()) { // 执行所有任务
      std::vector<int> current_run_ids = task[current_task_index];
      
      // 记录每个段的总大小和当前读取偏移
      std::vector<uint64_t> run_sizes(K);
      std::vector<uint64_t> run_offsets(K, 0);
      
      // 获取每个段的大小
      for(int i = 0; i < K; i++) {
         run_sizes[i] = in_store->get_run_size(current_run_ids[i]);
      }
      
      while(inited.load()) { // 等待本轮任务结束
         std::this_thread::yield();
      }
      // 初始化新的一轮 - 为每个段读取第一个缓冲区
      initializeRound(current_run_ids);
      inited.store(true); // 初始化完成
      
      // 更新初始偏移和last_key
      for(int i = 0; i < K; i++) {
         if(run_sizes[i] > 0) {
            size_t chunk_size = std::min(static_cast<size_t>(run_sizes[i]), 
                                        static_cast<size_t>(1 << 13)); // 8192个元素
            run_offsets[i] = chunk_size;
            // last_key记录已读取的元素数量
            (*last_key)[i] = chunk_size;
         }
      }
      
      // 持续补充数据直到所有段都读完
      while(true) { 
         // 检查是否所有段都已读完
         bool all_done = true;
         for(int i = 0; i < K; i++) {
            if(run_offsets[i] < run_sizes[i]) {
               all_done = false;
               break;
            }
         }
         if(all_done) break;
         
         // 找到last_key最小的且还未读完的段
         int min_index = -1;
         uint64_t min_key = UINT64_MAX;
         for(int i = 0; i < K; i++) {
            if(run_offsets[i] < run_sizes[i] && (*last_key)[i] < min_key) {
               min_key = (*last_key)[i];
               min_index = i;
            }
         }
         
         if(min_index == -1) break; // 没有可读的段, 结束本轮
         
         // 从缓冲池获取空闲缓冲区
         InputBuffer* buffer = pool->getBuffer();
         while(buffer == nullptr) {
            std::this_thread::yield(); // 等待归并线程归还缓冲区
            buffer = pool->getBuffer();
         }
         
         // 计算要读取的数据量
         uint64_t remaining = run_sizes[min_index] - run_offsets[min_index];
         uint64_t chunk_size = std::min(remaining, static_cast<uint64_t>(1 << 13)); // 8192个元素
         
         // 读取数据到缓冲区
         buffer->load_chunk(current_run_ids[min_index], 
                           run_offsets[min_index], 
                           chunk_size);
         
         // 更新偏移和last_key
         run_offsets[min_index] += chunk_size;
         (*last_key)[min_index] = run_offsets[min_index];
         
         // 将缓冲区加入对应的队列
         qs[min_index]->addBuffer(buffer);
      }
      
      // 当前任务完成，进入下一轮
      current_task_index++;
   }
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
