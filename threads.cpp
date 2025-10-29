#include <climits>
#include <cstdint>
#include <exception>
#include <thread>
#include <vector>
#include "buffer.h"
#include "run_store.h"
#include <queue>
#include <iostream>
#include "threads.h"
/*
    同步逻辑: buffer 自带一个 active 位, 
    当输入/输出线程准备操作的缓冲区的 active 为true时, yield() 让出cpu
    完成写入/输出后, 再改为true(可参与排序)
*/

std::vector<Task> initTasks(RunStore &store) {
    const size_t CHUNK = 65536; // 元素数
    std::vector<Task> tasks;
    for (uint32_t r = 0; r < store.run_count(); ++r) {
        uint64_t sz = store.get_run_size(r);
        for (uint64_t off = 0; off < sz; off += CHUNK) {
            uint64_t cnt = std::min<uint64_t>(CHUNK, sz - off);
            tasks.push_back({r, off, cnt});
        }
    }
    return tasks;
}

void reader(std::vector<InputBuffer> &bfs, 
            std::atomic<bool>& done_reading, 
            std::vector<Task>& tasks, 
            std::atomic<size_t>& next_task) {

    while(true) { 
        size_t idx = next_task.fetch_add(1);
        if (idx >= tasks.size()) break;
        auto t = tasks[idx];

        // 找到一个空闲的缓冲区
        InputBuffer* input = nullptr;
        while (input == nullptr) {
            if (!bfs[0].is_active()) {
                input = &bfs[0];
            } else if (!bfs[1].is_active()) {
                input = &bfs[1];
            } else {
                std::this_thread::yield();
            }
        }
        
        input->load_chunk(t.run_id, t.offset, t.count); 
        input->set_active(true);
    }
    done_reading.exchange(true);
}

void sorter(std::vector<InputBuffer>& inputs,
            std::vector<OutputBuffer>& outputs,
            const std::atomic<bool>& done_reading,
            std::atomic<bool>& done_sorting) {

    std::priority_queue<int64_t, std::vector<int64_t>, std::greater<int64_t>> min_heap;
    std::vector<int64_t> frozen;
    bool frozen_input = false;

    int cur_out = 0;
    size_t cur_out_count = 0;
    const size_t OUT_SWITCH_THRESHOLD = 1 << 16;

    while (true) {
        // 选择并填充输入缓冲区到堆中（如果堆为空）
        if (min_heap.empty()) {
            InputBuffer* current_input = nullptr;

            // 找到一个有数据的缓冲区
            if (inputs[0].is_active()) { 
                current_input = &inputs[0];
            } else if (inputs[1].is_active()) {
                current_input = &inputs[1];
            } else if (!frozen.empty()) {
                // 输入缓冲区都空了，但冻结区有数据 -> 利用冻结区重建堆
                std::cout << "Sorter: rebuilding heap from frozen, size=" << frozen.size() << std::endl;
                for (int64_t val : frozen) {
                    min_heap.push(val);
                }
                frozen.clear();
                continue;
            } else if (done_reading.load() && frozen.empty()) {
                // 所有输入已完成，且无冻结数据 -> 结束
                done_sorting.exchange(true);
                break;
            } else { // 等待缓冲区读入数据
                std::this_thread::yield();
                continue;
            }

            // 从选中的输入缓冲区填充堆
            while (current_input->has_next() && min_heap.size() < OUT_SWITCH_THRESHOLD) {
                min_heap.push(current_input->next());
            }

            // 标记输入缓冲区为空闲（可被 reader 重新填充）
            current_input->set_active(false);
        }

        // 输出堆顶，处理新元素 
        if (!min_heap.empty()) {
            int64_t last_output = LLONG_MIN;
            // 等待当前输出缓冲区可用
            while(outputs[cur_out].is_active()) {
                std::this_thread::yield();
            }
            
            while (!min_heap.empty()) {
                int64_t min_val = min_heap.top();
                min_heap.pop();

                outputs[cur_out].add(min_val);
                last_output = min_val;

                cur_out_count++;
                // 当即将启用frozen作为输入时，结束本段，因为接下来插入的数字较小
                if (frozen_input || cur_out_count >= OUT_SWITCH_THRESHOLD) {
                    // 数据写完，通知 writer
                    outputs[cur_out].set_active(true);
                    int next = 1 - cur_out;
                    cur_out = next;
                    cur_out_count = 0;
                    break;  // 跳出内层循环，切换到下一个缓冲区
                }

                while(!frozen.empty()) { // 将冻结区的加入最小堆
                    int64_t f = frozen.back();
                    frozen.pop_back();
                    min_heap.push(f);
                }
                if(frozen.empty())
                    frozen_input = false;
                // 尝试从输入获取新元素
                // 检查两个缓冲区，找到一个有数据的
                InputBuffer* src = nullptr;
                if (inputs[0].is_active() && inputs[0].has_next()) {
                    src = &inputs[0];
                } else if (inputs[1].is_active() && inputs[1].has_next()) {
                    src = &inputs[1];
                }
                
                if (src != nullptr) {
                    int64_t in_val = src->next();
                    if (in_val >= last_output) { 
                        min_heap.push(in_val);
                    } else { // 小于上一个输出的元素, 加入冻结区 
                        frozen.push_back(in_val);
                        if(frozen.size() > OUT_SWITCH_THRESHOLD) {
                            frozen_input = true;
                        }
                    }
                }
            }
        } else {
            std::this_thread::yield();
        }
    }

    // 确保所有剩余输出都被标记为 active，供 writer 处理
    if (cur_out_count > 0) {
        outputs[cur_out].set_active(true);
    }
}


void writer(std::vector<OutputBuffer> &bfs, 
            const std::atomic<bool>& done_sorting) {
    bool use_buffer_1 = true;

    while(!done_sorting.load()) {
        OutputBuffer& output = use_buffer_1? bfs[0] : bfs[1];
        
        // 等待缓冲区被标记为需要写入
        while(!output.is_active() && !done_sorting.load()) {
            std::this_thread::yield();
        }
        
        if(done_sorting.load()) break;

        output.flush_direct();
        output.set_active(false);
        
        use_buffer_1 = !use_buffer_1;
    }
    
    // 排序完成后，检查是否还有剩余数据需要 flush
    for (auto& buf : bfs) {
        if (buf.is_active() || !buf.empty()) {
            buf.flush_direct();
            buf.set_active(false);
        }
    }
}