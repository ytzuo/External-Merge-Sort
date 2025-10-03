#include "../include/TwoWayMerger.h"
#include <cstddef>

void TwoWayMerger::
MergeInMem(std::string input_file, std::string output_file, 
    int in1, int in2, int out1,int segNum) {
    InputBuffer& input1 = InputBuffers.at(in1);
    InputBuffer& input2 = InputBuffers.at(in2);
    OutputBuffer& output1 = OutputBuffers.at(out1);
    size_t offset1 = 0;
    size_t offset2 = 0;
    size_t size1 = input1.bufferSize();
    size_t size2 = input2.bufferSize();

    // 待比较的两个数
    input1.fillFromDisk(in1, offset1);
    offset1 += size1;
    input2.fillFromDisk(in2, offset2);
    offset2 += size2;

    int i1 = input1.getFromBuffer();
    int i2 = input2.getFromBuffer();
    /* 两个均未结束则二路归并 */
    while(!input1.is_finish() && input1.empty() && 
          !input2.is_finish() && input2.empty()) {
        /* 将较小的放入输出区 */
        if(i1 <= i2) {
            output1.pushToBuffer(i1);
            i1 = input1.getFromBuffer();
            if(input1.empty()) {
                input1.fillFromDisk(in1, offset1);
                offset1 += size1;
            }
        } else {
            output1.pushToBuffer(i2);
            i2 = input2.getFromBuffer();
            if(input2.empty()) {
                input2.fillFromDisk(in2, offset2);
                offset2 += size2;
            }
        }
        /* 如果缓冲区满了就写入磁盘 */
        if(output1.full()) 
            output1.appendSegToDisk();
    }

    while(!input1.is_finish() && input1.empty()) {
        output1.pushToBuffer(input1.getFromBuffer());
        if(input1.empty()) {
            input1.fillFromDisk(in1, offset1);
            offset1 += size1;
        }
        if(output1.full()) {
            output1.appendSegToDisk();
        }
    }
    while(!input2.is_finish() && input2.empty()) {
        output1.pushToBuffer(input2.getFromBuffer());
        if(input2.empty()) {
            input2.fillFromDisk(in2, offset2);
            offset2 += size2;
        }
        if(output1.full()) {
            output1.appendSegToDisk();
        }
    }
}

void TwoWayMerger::
ExternalMergeSort() {
    
}