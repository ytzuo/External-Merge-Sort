#include <vector>
#include "buffer.h"
/*
    同步逻辑: buffer 自带一个 active 位, 
    当输入/输出线程准备操作的缓冲区的 active 为true时, yield() 让出cpu
    完成写入/输出后, 再改为true(可参与排序)
*/
void reader(std::vector<InputBuffer> bfs) {

}

void sorter(std::vector<InputBuffer> inputs, std::vector<OutputBuffer> outputs) {

}

void writer(std::vector<OutputBuffer> bfs) {

}