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
    std::cout<<"开始将段 "<<in1<<" "<<in2<<" 进行排序"<<std::endl;
    InputBuffer& input1 = *InputBuffers.at(in1);
    InputBuffer& input2 = *InputBuffers.at(in2);
    OutputBuffer& output1 = *OutputBuffers.at(out1);
    output1.clear();
    size_t offset1 = 0;
    size_t offset2 = 0;
    size_t size1 = input1.bufferSize();
    size_t size2 = input2.bufferSize();

    // 填充初始缓冲区
    input1.fillFromDisk(in1, offset1);
    offset1 += size1;
    input2.fillFromDisk(in2, offset2);
    offset2 += size2;

    // 获取初始元素
    int i1 = input1.getFromBuffer();
    int i2 = input2.getFromBuffer();
    bool has_i1 = true;
    bool has_i2 = true;
    
    /* 二路归并主循环 */
    while(true) {
        /* 检查是否需要重新填充缓冲区 */
        if(input1.empty() && !input1.is_finish()) {
            input1.fillFromDisk(in1, offset1);
            offset1 += size1;
        }
        
        if(input2.empty() && !input2.is_finish()) {
            input2.fillFromDisk(in2, offset2);
            offset2 += size2;
        }
        
        /* 检查是否已经处理完所有数据 */
        if((input1.is_finish() && input1.empty() && !has_i1) && 
           (input2.is_finish() && input2.empty() && !has_i2)) {
            break;
        }
        
        /* 处理数据 */
        if(has_i1 && has_i2) {
            // 两个输入都还有数据
            if(i1 <= i2) {
                output1.pushToBuffer(i1);
                has_i1 = false;
                
                // 尝试获取下一个元素
                if (!input1.empty()) {
                    i1 = input1.getFromBuffer();
                    has_i1 = true;
                } else if (!input1.is_finish()) {
                    // 缓冲区为空但未完成，尝试填充
                    input1.fillFromDisk(in1, offset1);
                    offset1 += size1;
                    if (!input1.empty()) {
                        i1 = input1.getFromBuffer();
                        has_i1 = true;
                    }
                }
            } else {
                output1.pushToBuffer(i2);
                has_i2 = false;
                
                // 尝试获取下一个元素
                if (!input2.empty()) {
                    i2 = input2.getFromBuffer();
                    has_i2 = true;
                } else if (!input2.is_finish()) {
                    // 缓冲区为空但未完成，尝试填充
                    input2.fillFromDisk(in2, offset2);
                    offset2 += size2;
                    if (!input2.empty()) {
                        i2 = input2.getFromBuffer();
                        has_i2 = true;
                    }
                }
            }
        } else if(has_i1) {
            // 只有input1还有数据
            output1.pushToBuffer(i1);
            has_i1 = false;
            
            // 尝试获取下一个元素
            if (!input1.empty()) {
                i1 = input1.getFromBuffer();
                has_i1 = true;
            } else if (!input1.is_finish()) {
                // 缓冲区为空但未完成，尝试填充
                input1.fillFromDisk(in1, offset1);
                offset1 += size1;
                if (!input1.empty()) {
                    i1 = input1.getFromBuffer();
                    has_i1 = true;
                }
            }
        } else if(has_i2) {
            // 只有input2还有数据
            output1.pushToBuffer(i2);
            has_i2 = false;
            
            // 尝试获取下一个元素
            if (!input2.empty()) {
                i2 = input2.getFromBuffer();
                has_i2 = true;
            } else if (!input2.is_finish()) {
                // 缓冲区为空但未完成，尝试填充
                input2.fillFromDisk(in2, offset2);
                offset2 += size2;
                if (!input2.empty()) {
                    i2 = input2.getFromBuffer();
                    has_i2 = true;
                }
            }
        } else {
            // 两个都没有有效数据，尝试填充
            if (!input1.is_finish() && input1.empty()) {
                input1.fillFromDisk(in1, offset1);
                offset1 += size1;
                if (!input1.empty()) {
                    i1 = input1.getFromBuffer();
                    has_i1 = true;
                }
            }
            
            if (!input2.is_finish() && input2.empty()) {
                input2.fillFromDisk(in2, offset2);
                offset2 += size2;
                if (!input2.empty()) {
                    i2 = input2.getFromBuffer();
                    has_i2 = true;
                }
            }
            
            // 如果仍然没有数据且都已完成，则退出
            if ((input1.is_finish() && input1.empty()) && 
                (input2.is_finish() && input2.empty())) {
                break;
            }
        }
        
        /* 如果缓冲区满了就写入磁盘 */
        if(output1.full()) {
            std::cout<<"开始从input1和input2将Buffer输出到磁盘 "<<output1.size()<<std::endl;
            assert(output1.appendSegToDisk());
        }
    }

    if(!output1.empty()) {
        std::cout<<"开始将剩余的Buffer输出到磁盘 "<<output1.size()<<std::endl;
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
            //outputFile.close();
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
                InputBuffers[i]->fillFromDisk(i, 0);
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