#include "../include/TwoWayMerger.h"
#include <cstddef>
#include <memory>
#include <cassert>

void TwoWayMerger::
addInputBuffer(std::unique_ptr<InputBuffer> buffer) {
    InputBuffers.push_back(std::move(buffer));
}

void TwoWayMerger::
addOutputBuffer(std::unique_ptr<OutputBuffer> buffer) {
    OutputBuffers.push_back(std::move(buffer));
}

void TwoWayMerger::
MergeInMem(std::string input_file, std::string output_file, 
    int in1, int in2, int out1,int segNum) {
    InputBuffer& input1 = *InputBuffers.at(in1);
    InputBuffer& input2 = *InputBuffers.at(in2);
    OutputBuffer& output1 = *OutputBuffers.at(out1);
    output1.clear();
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
    while(!input1.is_finish() && !input1.empty() && 
          !input2.is_finish() && !input2.empty()) {
        /* 将较小的放入输出区 */
        if(i1 <= i2) {
            output1.pushToBuffer(i1);
            i1 = input1.getFromBuffer();
            if(input1.empty()) {
                std::cout<<"缓冲区 1 为空, 读取下一块"<<std::endl;
                input1.fillFromDisk(in1, offset1);
                offset1 += size1;
            }
        } else {
            output1.pushToBuffer(i2);
            i2 = input2.getFromBuffer();
            if(input2.empty()) {
                std::cout<<"缓冲区 2 为空, 读取下一块"<<std::endl;
                input2.fillFromDisk(in2, offset2);
                offset2 += size2;
            }
        }
        /* 如果缓冲区满了就写入磁盘 */
        //std::cout<<output1.size()<<std::endl;
        if(output1.full()) {
            std::cout<<"开始将Buffer输出到磁盘"<<std::endl;
            assert(output1.appendSegToDisk());
        }
    }

    while(!input1.is_finish() || !input1.empty()) {
        output1.pushToBuffer(input1.getFromBuffer());
        if(input1.empty()) {
            std::cout<<"缓冲区 1 为空, 读取下一块"<<std::endl;
            input1.fillFromDisk(in1, offset1);
            offset1 += size1;
        }
        //std::cout<<output1.size()<<std::endl;
        if(output1.full()) {
            std::cout<<"开始将Buffer输出到磁盘"<<std::endl;
            output1.appendSegToDisk();
        }
    }
    while(!input2.is_finish() || !input2.empty()) {
        output1.pushToBuffer(input2.getFromBuffer());
        if(input2.empty()) {
            std::cout<<"缓冲区 2 为空, 读取下一块"<<std::endl;
            input2.fillFromDisk(in2, offset2);
            offset2 += size2;
        }
        //std::cout<<output1.size()<<std::endl;
        if(output1.full()) {
            std::cout<<"开始将Buffer输出到磁盘"<<std::endl;
            output1.appendSegToDisk();
        }
    }

    if(!output1.empty()) {
        std::cout<<"开始将Buffer输出到磁盘"<<std::endl;
        output1.appendSegToDisk();
    }
}

void TwoWayMerger::
ExternalMergeSort(std::string initial_runs, size_t block_size) {
    // 1. 生成初始有序段（如果尚未完成）
    
    // 2. 多轮二路归并
    std::string currentFile = initial_runs;  // 包含初始有序段的文件
    int pass = 0;
    
    while (true) {
        MergeSortFile inputFile(currentFile);
        if (!inputFile.open()) {
            std::cerr << "无法打开文件: " << currentFile << std::endl;
            return;
        }
        
        auto segments = inputFile.getAllSegmentInfo();
        if (segments.size() <= 1) {
            // 排序完成
            inputFile.close();
            break;
        }
        
        // 创建输出文件
        std::string outputFile = "pass_" + std::to_string(pass) + ".msrt";
        MergeSortFile outputMsFile(outputFile);
        outputMsFile.create(outputFile, segments[0].count, true); // 假设所有段大小相同
        outputMsFile.close();

        // 创建输出缓冲区
        OutputBuffers.clear();
        auto outputBuffer = std::make_unique<OutputBuffer>(outputFile, block_size);
        addOutputBuffer(std::move(outputBuffer));

        // 创建输入缓冲区
        InputBuffers.clear();
        for (size_t i = 0; i < segments.size(); i++) {
            auto inputBuffer = std::make_unique<InputBuffer>(currentFile, block_size);
            addInputBuffer(std::move(inputBuffer));
        }
        
        // 对相邻的段进行二路归并
        for (size_t i = 0; i < segments.size(); i += 2) {
            if (i + 1 < segments.size()) {
                // 有两个段可以归并
                // 使用MergeInMem进行归并
                // 注意：需要正确设置输入输出缓冲区
                MergeInMem(currentFile, outputFile, i, i+1, 0, 2);
            } else {
                // 只有一个段，直接复制到输出文件
                while(!InputBuffers[i]->empty()) {
                    OutputBuffers[0]->pushToBuffer(InputBuffers[i]->getFromBuffer());
                }
                OutputBuffers[0]->appendSegToDisk();
            }
        }
        
        // 准备下一轮
        currentFile = outputFile;
        pass++;
        inputFile.close();
    }
    
    // 最终结果保存在currentFile中
}