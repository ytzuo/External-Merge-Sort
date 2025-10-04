#include <cstddef>
#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>
#include "../include/TwoWayMerger.h"

void testExternalMergeSort() {
    std::cout << "开始 testExternalMergeSort()" << std::endl;
    size_t block_size = 1024;
    std::string rawFile = "raw_for_ext_sort.msrt";
    std::string runFile = "run_for_ext_sort.msrt";

    // 生成原始数据
    std::cout << "开始生成原始数据" << std::endl;
    MergeSortFile raw(rawFile);
    assert(raw.genRawData(rawFile, 2*1024, 2*1024)); // 生成更大的数据集以便测试多轮归并
    if(raw.is_open())
        raw.close();
    std::cout << "原始数据生成完成" << std::endl;

    // 生成初始有序段
    std::cout << "开始生成初始有序段" << std::endl;
    assert(genRun(rawFile, runFile, block_size));
    std::cout << "初始有序段生成完成" << std::endl;

    // 检查初始有序段
    std::cout << "开始检查初始有序段" << std::endl;
    MergeSortFile run(runFile);
    if (!run.open()) {
        std::cout << "无法打开初始有序段文件" << std::endl;
        return;
    }
    
    auto segments = run.getAllSegmentInfo();
    std::cout << "初始有序段总数: " << segments.size() << std::endl;
    
    // 打印所有段的元数据信息
    for (size_t i = 0; i < segments.size(); i++) {
        std::cout << "段 " << i << " offset: " << segments[i].offset
                  << ", length: " << segments[i].length
                  << ", count: " << segments[i].count << std::endl;
    }
    run.close();
    
    // 执行外排序
    std::cout << "开始执行外排序" << std::endl;
    TwoWayMerger merger;
    merger.ExternalMergeSort(runFile, block_size);
    std::cout << "外排序执行完成" << std::endl;
    
    // 检查最终结果
    // 根据TwoWayMerger.cpp中的实现，最终结果应该在最后一轮生成的文件中
    // 我们需要找到最终的输出文件
    std::string finalFile = "pass_0.msrt"; // 至少会有一轮
    // 检查是否存在更多的pass文件
    int pass = 1;
    while (std::filesystem::exists("pass_" + std::to_string(pass) + ".msrt")) {
        finalFile = "pass_" + std::to_string(pass) + ".msrt";
        pass++;
    }
    
    std::cout << "最终排序结果文件: " << finalFile << std::endl;
    
    MergeSortFile result(finalFile);
    if (!result.open()) {
        std::cout << "无法打开最终结果文件" << std::endl;
        return;
    }
    
    auto finalSegments = result.getAllSegmentInfo();
    std::cout << "最终段总数: " << finalSegments.size() << std::endl;
    
    if (!finalSegments.empty()) {
        // 读取最终排序结果
        std::vector<int> buffer;
        if (result.readSegment(0, buffer)) {
            std::cout << "最终排序结果大小: " << buffer.size() << std::endl;
            
            // 验证结果是否已排序
            bool isSorted = std::is_sorted(buffer.begin(), buffer.end());
            std::cout << "排序结果" << (isSorted ? "正确" : "错误") << std::endl;
            
            // 打印前几个和后几个元素
            std::cout << "前10个元素: ";
            for(size_t j = 0; j < buffer.size() && j < 10; j++) {
                std::cout << buffer.at(j) << " ";
            }
            if (buffer.size() > 10) std::cout << "...";
            std::cout << std::endl;
            
            std::cout << "后10个元素: ";
            if (buffer.size() > 10) {
                for(size_t j = buffer.size() - 10; j < buffer.size(); j++) {
                    std::cout << buffer.at(j) << " ";
                }
            }
            std::cout << std::endl;
        } else {
            std::cout << "无法读取最终排序结果" << std::endl;
        }
    }
    
    result.close();
    std::cout << "最终结果检查完成" << std::endl;

    // 清理测试文件
    std::filesystem::remove(rawFile);
    std::filesystem::remove(runFile);
    
    // 清理中间文件
    pass = 0;
    while (std::filesystem::exists("pass_" + std::to_string(pass) + ".msrt")) {
        std::filesystem::remove("pass_" + std::to_string(pass) + ".msrt");
        pass++;
    }
    
    std::cout << "测试文件清理完成" << std::endl;
}

int main() {
    testExternalMergeSort();
    return 0;
}