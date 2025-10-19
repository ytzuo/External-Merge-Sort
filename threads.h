#pragma once
#include "buffer.h"
#include <cstdint>
#include <vector>
/*
    InputBuffer --输入优先队列--> 优先队列(Heap) --队列头部输入缓冲区--> OutputBuffer
    好像期望上可以生成缓冲区长度两倍的 run

    第一次读取将所有的都插入优先队列, 接下来的读取在优先队列出现空缺时插入一个进入队列
*/
void reader(std::vector<InputBuffer> bfs);

void sorter(std::vector<InputBuffer> inputs, std::vector<OutputBuffer> outputs);

void writer(std::vector<OutputBuffer> bfs);