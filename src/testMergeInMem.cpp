#include <cstddef>
#include <iostream>
#include <cassert>
#include <vector>
#include "../include/TwoWayMerger.h"
#include <memory>

void testMergeInMem() {
    std::cout << "开始 testMergeInMem()" << std::endl;
    size_t block_size = 512;
    std::string rawFile = "raw.msrt";
    std::string run1File = "run1.msrt";
    std::string run2File = "run2.msrt";
    std::string mergedFile = "merged.msrt";

    // 生成原始数据
    std::cout << "开始生成原始数据" << std::endl;
    MergeSortFile raw(rawFile);
    assert(raw.genRawData(rawFile, 2*1024, 2*1024));
    if(raw.is_open())
        raw.close();
    std::cout << "原始数据生成完成" << std::endl;

    // 生成两个归并段文件
    std::cout << "开始生成第一个归并段" << std::endl;
    assert(genRun(rawFile, run1File, block_size));
    std::cout << "第一个归并段生成完成" << std::endl;

    std::cout << "开始生成第二个归并段" << std::endl;
    assert(genRun(rawFile, run2File, block_size));
    std::cout << "第二个归并段生成完成" << std::endl;

    // 创建输出文件
    std::cout << "创建输出文件" << std::endl;
    MergeSortFile outputFile(mergedFile);
    outputFile.create(mergedFile, block_size, true);
    outputFile.close();
    std::cout << "输出文件创建完成" << std::endl;

    // 创建TwoWayMerger实例并添加输入输出缓冲区
    TwoWayMerger merger;
    
    // 添加输入缓冲区
    auto inputBuffer1 = std::make_unique<InputBuffer>(run1File, block_size);
    auto inputBuffer2 = std::make_unique<InputBuffer>(run2File, block_size);
    merger.addInputBuffer(std::move(inputBuffer1));
    merger.addInputBuffer(std::move(inputBuffer2));
    
    // 添加输出缓冲区
    auto outputBuffer = std::make_unique<OutputBuffer>(mergedFile, block_size);
    merger.addOutputBuffer(std::move(outputBuffer));
    
    // 执行内存中的二路归并
    std::cout << "开始执行内存中的二路归并" << std::endl;
    merger.MergeInMem(run1File, mergedFile, 0, 1, 0, 2);
    std::cout << "内存中的二路归并完成" << std::endl;

    // 检查合并结果
    std::cout << "开始检查合并结果" << std::endl;
    MergeSortFile merged(mergedFile);
    if (!merged.open()) {
        std::cout << "无法打开合并后的文件" << std::endl;
        return;
    }
    
    auto segments = merged.getAllSegmentInfo();
    std::cout << "合并后段总数: " << segments.size() << std::endl;
    
    // 打印所有段的元数据信息
    for (size_t i = 0; i < segments.size(); i++) {
        std::cout << "段 " << i << " offset: " << segments[i].offset
                  << ", length: " << segments[i].length
                  << ", count: " << segments[i].count << std::endl;
    }
    
    for(size_t i = 0; i < segments.size() && i < 2; i++) {
        std::vector<int> buffer;
        if (merged.readSegment(i, buffer)) {
            std::cout << "段 " << i << " 大小: " << buffer.size() << std::endl;
            // 只打印前20个元素
            for(size_t j = 0; j < buffer.size() && j < 20; j++) {
                std::cout << buffer.at(j) << " ";
            }
            if (buffer.size() > 10) std::cout << "...";
            std::cout << std::endl;
        } else {
            std::cout << "无法读取段 " << i << std::endl;
        }
    }
    std::cout << "合并结果检查完成" << std::endl;

    // 清理临时文件
    std::filesystem::remove(rawFile);
    std::filesystem::remove(run1File);
    std::filesystem::remove(run2File);
    std::filesystem::remove(mergedFile);
}
/*
int main() {
    testMergeInMem();
    return 0;
}
*/