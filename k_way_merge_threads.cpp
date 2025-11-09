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
            RunStore* out_store,
            std::vector<std::vector<int>> task,
            std::atomic<bool>& inited)
    : K(K), qs(qs), last_key(&last_key), outs(outs), pool(pool), 
      out_store(out_store), task(task), inited(inited){}


void MergeThread::
kWayMerge(size_t task_num, std::atomic<bool>& done_sorting) {

   // 初始化最小堆
   std::priority_queue<std::pair<int64_t, int>, 
                       std::vector<std::pair<int64_t, int>>, 
                       std::greater<std::pair<int64_t, int>>> pq;
   std::vector<InputBuffer*> current_buffers(K, nullptr); // 当前正在使用的缓冲区
   int cur_out  = 0;
   size_t cur_out_count = 0;
   const size_t OUT_SWITCH_THRESHOLD = 1 << 16;
   int cur_task = 0;
   bool run_started = false; // ⭐ 记录本轮是否已经开始run
   //uint64_t cur_out_count = 0;

   while(cur_task < task_num) {
      while(!inited.load()) {
         std::this_thread::yield();
      }
      std::cout<<"开始处理第"<<cur_task<<"轮"<<std::endl;

      int queue_num = task[cur_task].size();
      for(int i = 0; i < queue_num; i++) { // 初始化最小堆
         //std::cout<<"MergeThread: 正在从队列"<<i<<"获取缓冲区..."<<std::endl;
         current_buffers[i] = qs[i]->getBuffer(); // 取出队列头部的缓冲区
         //std::cout<<"MergeThread: 从队列"<<i<<"获取缓冲区成功，加入堆..."<<std::endl;
         pq.push(std::make_pair(current_buffers[i]->next(), i));
         //std::cout<<"MergeThread: 队列"<<i<<"的第一个元素已加入堆"<<std::endl;
      }
      std::cout<<"MergeThread: 堆初始化完成，开始归并循环..."<<std::endl;
      
      // ⭐ 一轮开始时，调用一次 begin_run()
      out_store->begin_run();
      run_started = true;
      
      // 当所有队列都不为空: 每次从最小堆里弹出一个, 并从对应的缓冲区中读取下一个元素
      // 当一个缓冲区为空, 确认该缓冲区是否已经读完, 如果没有则等待输入线程填充数据
      // 已经读完则继续
      while(true) {
         // 检查有没有空的缓冲区
         int finished_run_count = 0;
         
         for(int i = 0; i < queue_num; i++) {
            if(current_buffers[i] == nullptr) { // 当前某段没有缓冲区正在使用
               bool is_empty = qs[i]->empty();
               bool has_next = qs[i]->has_next();
               
               if(is_empty && !has_next) { 
                  // 对应缓冲区队列也为空, 且也没有需要读取的数据
                  finished_run_count++;
                  continue; // 说明已经读取完成, 跳过这个缓冲区
               } else if (!is_empty) {
                  // 还有缓冲区则加载一个缓冲区
                  current_buffers[i] = qs[i]->getBuffer();
                  // ⚠️ 关键：需要将新缓冲区的第一个元素加入堆！
                  if(current_buffers[i] != nullptr && !current_buffers[i]->empty()) {
                     pq.push(std::make_pair(current_buffers[i]->next(), i));
                  }
               } else if (has_next){ 
                  // 段中还有需要读取的
                  while(qs[i]->has_next() && current_buffers[i] == nullptr)
                     std::this_thread::yield();
                  i--;
               }
            }
         }
         if(finished_run_count == queue_num)
            break;

         while(!pq.empty()) {
            //std::cout<<"开始将结果输出到输出缓冲区"<<std::endl;
            std::pair<int64_t, int> top = pq.top();
            int64_t val = top.first;
            int src = top.second;
            pq.pop();

            while(outs[cur_out]->is_active()) { // 等待缓冲区可用
               static int wait_count = 0;
               // if(wait_count++ % 100000000 == 0) {
               //    std::cout<<"MergeThread: 等待OutputBuffer["<<cur_out<<"] available..."<<std::endl;
               // }
               std::this_thread::yield();
            }
            outs[cur_out]->add(val);
            cur_out_count++;
            if(cur_out_count >= OUT_SWITCH_THRESHOLD) {
               outs[cur_out]->set_active(true);
               cur_out = 1 - cur_out;
               cur_out_count = 0;
            }

            if(!current_buffers[src]->empty()) { // 缓冲区还有数据
               // 从对应缓冲区队列加入一个数据到最小堆
               pq.push(std::make_pair(current_buffers[src]->next(), src));
            } else { // 移除缓冲区并还到 pool
               pool->returnBuffer(current_buffers[src]);
               current_buffers[src] = nullptr;
            }
         }
         for(int i = 0; i < queue_num; i++) { // 防止还有未归还的缓冲区
            if(current_buffers[i] != nullptr && current_buffers[i]->empty()) {
               pool->returnBuffer(current_buffers[i]);
               current_buffers[i] = nullptr;
            }
         }
      }
      // 一轮结束, 回到未初始化状态
      std::cout<<"第"<<cur_task<<"轮结束"<<std::endl;
      
      // 标记当前缓冲区为active，通知Writer线程写入
      if(cur_out_count > 0) {
          outs[cur_out]->set_active(true);
      }
      
      // 等待Writer线程完成所有写入
      while(outs[0]->is_active() || outs[1]->is_active()) {
          std::this_thread::yield();
      }
      
      // flush所有缓冲区的剩余数据（使用flush_direct，不调用end_run）
      for(auto* out : outs) {
          if(!out->empty()) {
              out->flush_direct();
          }
      }
      
      // ⭐ 一轮结束，调用一次 end_run() 创建目录条目
      if(run_started) {
          out_store->end_run();
          run_started = false;
      }
      
      // 获取当前总run数
      uint32_t total_runs = out_store->run_count();
      std::cout<<"第"<<cur_task<<"轮结束，当前总共"<<total_runs<<"个run"<<std::endl;
      
      inited.store(false);
      cur_task ++;
   }
   done_sorting.store(true); // 通知写入线程已经排序完毕
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
      // ⭐ 先设置run_size，再添加buffer
      qs[i]->setRunSize(run_size);
      
      // 计算第一块要读取的大小
      uint64_t chunk_size = std::min(run_size, static_cast<uint64_t>(1 << 13));
      // 读取第一块数据
      if(chunk_size > 0) {
         buffer->load_chunk(run_ids[i], 0, chunk_size);
      }
      std::cout<<"初始化缓冲区, queue_id = "<<i
               <<", run_id = "<<run_ids[i]<<", run_size = "
               <<run_size<<std::endl;
      // 加入队列
      qs[i]->addBuffer(buffer);
      // ⭐ 更新已读取的元素数
      qs[i]->addElementsRead(chunk_size);
   }
}

InputThread::
InputThread(int K,
            RunStore* store,
            BufferPool* pool,
            std::vector<BufferQueue*>& qs,
            std::vector<int64_t>& last_key,
            std::vector<std::vector<int>> task,
            std::atomic<bool>& inited)
    : K(K), in_store(store), pool(pool), qs(qs),
    last_key(&last_key),
    inited(inited),
    task(std::move(task)) {}

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
            (*last_key)[i] = 0;
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
         // ⭐ 更新已读取的元素数
         qs[min_index]->addElementsRead(chunk_size);
      }
      
      // 当前任务完成，进入下一轮
      current_task_index++;
   }
}
