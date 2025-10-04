#pragma once
#include "MergeSortFile.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstddef>
#include <iostream>
#include "../include/Buffer.h"
#include <memory>

class TwoWayMerger {
private:
    /* 输入输出 Buffer 池 */
    std::vector<std::unique_ptr<InputBuffer>>  InputBuffers;
    std::vector<std::unique_ptr<OutputBuffer>> OutputBuffers;
public:
    /* 将 Buffer 加入 Buffer池 */
    void addInputBuffer(std::unique_ptr<InputBuffer> buffer);
    void addOutputBuffer(std::unique_ptr<OutputBuffer> buffer);

    /* 内存中二路归并 -- 二输入二输出 */
    void MergeInMem(std::string input_file, std::string output_file, 
        int input1, int input2, int output1, int segNum);

    /* 基于二路归并的外排序算法 */
    void ExternalMergeSort(std::string initial_runs, size_t block_size);
};

inline bool genRun(std::string filename, std::string new_file, size_t block_size) {
    try{
        MergeSortFile file(filename);
        if (!file.open()) return false;
        std::cout<<"成功打开初始文件"<<std::endl;

        MergeSortFile runFile(new_file);
        if (!runFile.create(new_file, block_size, false)) return false;
        if (!runFile.open()) return false;
        std::cout<<"成功创建新文件"<<std::endl;

        std::vector<int> temp;
        size_t startOffset = 0;
        bool hasData = false;
        // 只要返回值一直为true;
        std::cout<<"开始读取初始文件"<<std::endl;
        //runFile.writeHeader(block_size);
        while(file.readSegmentChunk(0, startOffset, block_size, temp)) {
            hasData = true;
            // 排序, 生成初始有序run
            //std::cout<<"temp.size(): "<<temp.size()<<std::endl;
            std::sort(temp.begin(), temp.end());
            // 写回排序好的run
            if (!runFile.appendSegment(temp)) {
                std::cout<<"追加段失败"<<std::endl;
                return false;
            }
            startOffset += block_size;
            temp.clear();
        }
        
        if (!hasData) {
            std::cout<<"警告：未从初始文件读取到任何数据"<<std::endl;
            return false;
        }

        if(file.is_open())
            file.close();
        if(runFile.is_open())
            runFile.close();
        return true;
    } catch(const std::exception& e) {
        std::cout<<"异常: "<<e.what()<<std::endl;
        return false;
    }
}