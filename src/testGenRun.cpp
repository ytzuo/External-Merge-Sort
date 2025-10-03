#include <cstddef>
#include <iostream>
#include <cassert>
#include <vector>
#include "../include/TwoWayMerger.h"

void testGenRun() {
    std::cout<<"开始 testGenRun()"<<std::endl;
    size_t block_size = 1024;
    std::string rawFile = "raw.msrt";
    std::string runFile = "run.msrt";

    // 生成原始数据
    std::cout<<"开始生成原始数据"<<std::endl;
    MergeSortFile raw(rawFile);
    assert(raw.genRawData(rawFile, 4*1024, 4*1024));
    if(raw.is_open())
        raw.close();
    std::cout<<"原始数据生成完成"<<std::endl;

    // 生成归并段
    std::cout<<"开始生成归并段"<<std::endl;
    assert(genRun(rawFile, runFile, block_size));
    std::cout<<"归并段生成完成"<<std::endl;

    // 检查归并段
    std::cout<<"开始检查归并段"<<std::endl;
    MergeSortFile run(runFile);
    if (!run.open()) {
        std::cout<<"无法打开归并段文件"<<std::endl;
        return;
    }
    
    auto segments = run.getAllSegmentInfo();
    std::cout<<"归并段总数: "<<segments.size()<<std::endl;
    
    // 打印所有段的元数据信息
    for (size_t i = 0; i < segments.size(); i++) {
        std::cout<<"段 "<<i<<" offset: "<<segments[i].offset
                 <<", length: "<<segments[i].length
                 <<", count: "<<segments[i].count<<std::endl;
    }
    
    for(size_t i = 0; i < segments.size(); i++) {
        std::vector<int> buffer;
        if (run.readSegment(i, buffer)) {
            std::cout<<"段 "<<i<<" 大小: "<<buffer.size()<<std::endl;
            // 只打印前10个元素
            for(size_t j = 0; j < buffer.size() && j < 10; j++) {
                std::cout<<buffer.at(j)<<" ";
            }
            if (buffer.size() > 10) std::cout<<"...";
            std::cout<<std::endl;
        } else {
            std::cout<<"无法读取段 "<<i<<std::endl;
        }
    }
    std::cout<<"归并段检查完成"<<std::endl;

    std::filesystem::remove(rawFile);
    std::filesystem::remove(runFile);
}

/*
int main() {
    testGenRun();
    return 0;
}
*/