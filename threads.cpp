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
                        
    // 改进的同步逻辑框架
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
    
    // TODO: 实现完整的排序逻辑，包括交替使用两个输入缓冲区
    /*
    bool use_buffer_1 = false; // 初始使用buffer1已完成
    while (in1.has_next() || in2.has_next()) {
        // 检查并使用就绪的缓冲区
        if (!use_buffer_1 && !in2.is_active() && in2.has_next()) {
            // 使用buffer2
            in2.set_active(true); // 标记为使用中
            process_buffer_data(in2, min_heap);
            in2.set_active(false); // 标记为空闲
            use_buffer_1 = true;
        } 
        else if (use_buffer_1 && !in1.is_active() && in1.has_next()) {
            // 使用buffer1
            in1.set_active(true); // 标记为使用中
            process_buffer_data(in1, min_heap);
            in1.set_active(false); // 标记为空闲
            use_buffer_1 = false;
        }
        std::this_thread::yield();
    }
    */
}

// 辅助函数：处理缓冲区数据
void process_buffer_data(InputBuffer& buffer, std::priority_queue<int64_t, 
                        std::vector<int64_t>, 
                        std::greater<int64_t>>& min_heap) {
    // 从缓冲区读取数据并处理
    while(buffer.has_next()) {
        min_heap.push(buffer.next());
        // 可以在这里添加其他处理逻辑
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