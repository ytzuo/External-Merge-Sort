#include <climits>
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
            bool use_buffer_1 = true;

    // TODO: 实现读取逻辑，交替填充两个缓冲区
    while(true) { // 这里还要换成真正的结束条件(读取已经到达文件末尾)
        size_t idx = next_task.fetch_add(1);
        if (idx >= tasks.size()) break; // 任务全部完成
        auto t = tasks[idx];

        InputBuffer& input = use_buffer_1 ? bfs[0] : bfs[1];
        while(input.is_active()) {
            std::this_thread::yield();
        }
        input.set_active(true);
        input.load_chunk(t.run_id, t.offset, t.count); 
        input.set_active(false);
    }
    done_reading.exchange(true);
}

/*
    还要加上对于生成的初始归并段的处理逻辑:
    当开始时创建新的段, 当堆的顶端插不进输出缓冲区时(小于缓冲区的最后一个数字) 关闭段
    可能需要修改排序逻辑
*/

void sorter(std::vector<InputBuffer>& inputs,
            std::vector<OutputBuffer>& outputs,
            const std::atomic<bool>& done_reading,
            std::atomic<bool>& done_sorting) {

    std::priority_queue<int64_t, std::vector<int64_t>, std::greater<int64_t>> min_heap;
    std::vector<int64_t> frozen; // 冻结区, 用于存储小于上一个输出元素的输入元素

    int cur_out = 0;
    size_t cur_out_count = 0;
    const size_t OUT_SWITCH_THRESHOLD = 1 << 16; // 输出缓冲区切换阈值 这里不设置为满, 
                                                 // 因为原本的flush会生成新的run, 需要写一个新的写入逻辑

    bool use_input_1 = true; // 交替使用输入缓冲区

    while (true) {
        // 选择并填充输入缓冲区到堆中（如果堆为空）
        if (min_heap.empty()) {
            InputBuffer* current_input = nullptr;

            if (use_input_1 && !inputs[0].is_active() && inputs[0].has_next()) { 
                // 缓冲区1还有数据
                inputs[0].set_active(true);
                current_input = &inputs[0];
            } else if (!use_input_1 && !inputs[1].is_active() && inputs[1].has_next()) {
                // 缓冲区2还有数据
                inputs[1].set_active(true);
                current_input = &inputs[1];
            } else if (!frozen.empty()) {
                // 输入缓冲区都空了，但冻结区有数据 → 重建堆
                for (int64_t val : frozen) {
                    min_heap.push(val);
                }
                frozen.clear();
                use_input_1 = !use_input_1; // 切换输入源
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
            use_input_1 = !use_input_1;
        }

        // 输出堆顶，处理新元素 
        if (!min_heap.empty()) {
            int64_t last_output = LLONG_MIN;

            while (!min_heap.empty()) {
                int64_t min_val = min_heap.top();
                min_heap.pop();

                outputs[cur_out].add(min_val);
                last_output = min_val;

                cur_out_count++;
                if (cur_out_count >= OUT_SWITCH_THRESHOLD) {
                    outputs[cur_out].set_active(true); // 通知 writer
                    int next = 1 - cur_out;
                    while (outputs[next].is_active()) { // 下一个使用的输出缓冲区还在写入
                        std::this_thread::yield();
                    }
                    cur_out = next;
                    cur_out_count = 0;
                }

                // 尝试从输入获取新元素（需要再次检查输入）
                InputBuffer* src = use_input_1 ? &inputs[0] : &inputs[1];
                if (!src->is_active() && src->has_next()) {
                    int64_t in_val = src->next();
                    if (in_val >= last_output) { 
                        min_heap.push(in_val);
                    } else { // 小于上一个输出的元素, 加入冻结区 
                        // 可能还需要加上这样的逻辑: 当冻结区域过大, 直接把缓冲区一口气输出完
                        frozen.push_back(in_val);
                    }
                }
                // 注意：这里简化了输入获取逻辑，实际可能需要更复杂的轮询
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

    while(!bfs[0].empty() && !bfs[1].empty() && !done_sorting.load()) { // 这里要修改为真实结束逻辑
        OutputBuffer& output = use_buffer_1? bfs[0] : bfs[1];
        
        while(output.is_active()) {
            std::this_thread::yield();
        }

        output.set_active(true);
        //output.flush();  // 这个地方也不能直接调用flush, 可能需要只写入, 不统计段长度
        output.flush_direct(); // 现在是直接写入
        output.set_active(false);
    }
}