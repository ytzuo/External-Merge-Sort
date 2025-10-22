#include <thread>
#include <vector>
#include "buffer.h"
#include <queue>
#include <iostream>
/*
    同步逻辑: buffer 自带一个 active 位, 
    当输入/输出线程准备操作的缓冲区的 active 为true时, yield() 让出cpu
    完成写入/输出后, 再改为true(可参与排序)
*/
void reader(std::vector<InputBuffer> &bfs) {
    int buffer_num = bfs.size(); // 应该只会是两个
    InputBuffer& in1 = bfs[0];
    InputBuffer& in2 = bfs[1];
    
    // 初始化缓冲区状态
    in1.set_active(false);
    in2.set_active(false);
    
    // TODO: 实现读取逻辑，交替填充两个缓冲区
    // 示例框架：
    /*
    bool use_buffer_1 = true;
    while (has_more_data()) {
        if (use_buffer_1) {
            // 等待buffer1变为空闲
            while (in1.is_active()) {
                std::this_thread::yield();
            }
            // 标记buffer1为使用中
            in1.set_active(true);
            // 填充buffer1数据
            fill_buffer(in1);
            // 标记buffer1为就绪
            in1.set_active(false);
        } else {
            // 等待buffer2变为空闲
            while (in2.is_active()) {
                std::this_thread::yield();
            }
            // 标记buffer2为使用中
            in2.set_active(true);
            // 填充buffer2数据
            fill_buffer(in2);
            // 标记buffer2为就绪
            in2.set_active(false);
        }
        use_buffer_1 = !use_buffer_1;
    }
    */
}

/*
    还要加上对于生成的初始归并段的处理逻辑:
    当开始时创建新的段, 当堆的顶端插不进输出缓冲区时(小于缓冲区的最后一个数字) 关闭段
    可能需要修改排序逻辑
*/
void process_buffer_data(InputBuffer& input, 
                                 std::vector<OutputBuffer>& outputs,
                                 std::priority_queue<int64_t, 
                                     std::vector<int64_t>, 
                                     std::greater<int64_t>>& min_heap,
                                 int &cur_out, size_t &cur_out_count, size_t out_switch_threshold);


void sorter(std::vector<InputBuffer> &inputs, std::vector<OutputBuffer> &outputs) {
    int input_num      = inputs.size();  // 应该只会是两个
    int output_num     = outputs.size(); // 应该只会是两个
    InputBuffer& in1   = inputs[0];
    InputBuffer& in2   = inputs[1];
    OutputBuffer& out1 = outputs[0];
    OutputBuffer& out2 = outputs[1];

    /* 一个堆, 用于生成归并段的核心数据结构 */
    std::priority_queue<int64_t, 
                        std::vector<int64_t>, 
                        std::greater<int64_t>> min_heap;
                        
    // 等待第一个缓冲区变为空闲状态
    while(in1.is_active()) 
        std::this_thread::yield(); 
    // 标记第一个缓冲区为使用中
    in1.set_active(true);
    // 从第一个缓冲区读取数据填充最小堆
    while(in1.has_next()) {
        min_heap.push(in1.next());
    }
    // 标记第一个缓冲区为空闲状态
    in1.set_active(false);
    
    // 完整的排序逻辑：交替使用两个输入缓冲区，并交替输出到两个输出缓冲区
    const size_t OUT_SWITCH_THRESHOLD = 1 << 16; // 达到该数量切换输出缓冲区

    // 启动写线程，监听 output buffers 的 active 标志并执行 flush
    std::atomic<bool> done{false};
    std::thread writer_thread([&outputs, &done]() {
        OutputBuffer &o1 = outputs[0];
        OutputBuffer &o2 = outputs[1];
        while (!done.load()) {
            if (o1.is_active()) {
                o1.flush();
                o1.set_active(false);
            }
            if (o2.is_active()) {
                o2.flush();
                o2.set_active(false);
            }
            std::this_thread::yield();
        }
        // 结束前再次 flush
        if (o1.is_active()) { o1.flush(); o1.set_active(false); }
        if (o2.is_active()) { o2.flush(); o2.set_active(false); }
    });

    // 当前输出索引与计数
    int cur_out = 0;
    size_t cur_out_count = 0;

    bool use_input_1 = false; // 初始使用buffer1已完成
    while (in1.has_next() || in2.has_next()) {
        if (!use_input_1 && !in2.is_active() && in2.has_next()) {
            in2.set_active(true);
            process_buffer_data(in2, outputs, min_heap, cur_out, cur_out_count, OUT_SWITCH_THRESHOLD);
            in2.set_active(false);
            use_input_1 = true;
        } else if (use_input_1 && !in1.is_active() && in1.has_next()) {
            in1.set_active(true);
            process_buffer_data(in1, outputs, min_heap, cur_out, cur_out_count, OUT_SWITCH_THRESHOLD);
            in1.set_active(false);
            use_input_1 = false;
        } else {
            std::this_thread::yield();
        }
    }

    // 将堆中剩余元素写出
    while (!min_heap.empty()) {
        int64_t v = min_heap.top(); min_heap.pop();
        outputs[cur_out].add(v);
        cur_out_count++;
        if (cur_out_count >= OUT_SWITCH_THRESHOLD) {
            outputs[cur_out].set_active(true);
            int next = 1 - cur_out;
            // 等待另一个输出缓冲区可用
            while (outputs[next].is_active()) std::this_thread::yield();
            cur_out = next;
            cur_out_count = 0;
        }
    }

    // 通知写线程结束并触发最终 flush
    done.store(true);
    outputs[0].set_active(true);
    outputs[1].set_active(true);
    writer_thread.join();
    
}

// 辅助函数：处理缓冲区数据，向两个输出缓冲区交替写入
void process_buffer_data(InputBuffer& input, 
                         std::vector<OutputBuffer>& outputs,
                         std::priority_queue<int64_t, 
                            std::vector<int64_t>, 
                            std::greater<int64_t>>& min_heap,
                         int &cur_out, size_t &cur_out_count, size_t out_switch_threshold) {
    // 从缓冲区读取数据并处理
    while(input.has_next()) {
        int64_t in_val = input.next();
        // 将最小堆的最小值输出
        if (!min_heap.empty()) {
            outputs[cur_out].add(min_heap.top());
            min_heap.pop();
            min_heap.push(in_val);
        } else {
            // 如果堆为空，直接把输入放入堆
            min_heap.push(in_val);
        }

        cur_out_count++;
        if (cur_out_count >= out_switch_threshold) {
            // 通知写线程 flush 当前输出
            outputs[cur_out].set_active(true);
            // 切换到另一个输出缓冲区
            int next = 1 - cur_out;
            // 等待另一个输出变为可用（不被写线程占用）
            while (outputs[next].is_active()) std::this_thread::yield();
            cur_out = next;
            cur_out_count = 0;
        }
    }
}

void writer(std::vector<OutputBuffer> &bfs) {
    int buffer_num = bfs.size(); // 应该只会是两个
    OutputBuffer& out1 = bfs[0];
    OutputBuffer& out2 = bfs[1];
    
    // 初始化输出缓冲区状态
    out1.set_active(false);
    out2.set_active(false);
    
    // TODO: 实现写入逻辑，交替使用两个输出缓冲区
    // 示例框架：
    /*
    bool use_buffer_1 = true;
    while (has_more_data_to_write()) {
        if (use_buffer_1) {
            // 等待buffer1变为空闲
            while (out1.is_active()) {
                std::this_thread::yield();
            }
            // 标记buffer1为使用中
            out1.set_active(true);
            // 写入数据到buffer1
            write_to_buffer(out1);
            // 标记buffer1为就绪
            out1.set_active(false);
        } else {
            // 等待buffer2变为空闲
            while (out2.is_active()) {
                std::this_thread::yield();
            }
            // 标记buffer2为使用中
            out2.set_active(true);
            // 写入数据到buffer2
            write_to_buffer(out2);
            // 标记buffer2为就绪
            out2.set_active(false);
        }
        use_buffer_1 = !use_buffer_1;
    }
    */
}