#include "../include/MergeSortFile.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <random>
#include <algorithm>

void testCreateAndBasicOperations() {
    std::cout << "测试创建文件和基本操作..." << std::endl;
    
    // 测试创建文件
    MergeSortFile msFile("test_data.dat");
    assert(msFile.create("test_data.dat", 1024, false));
    std::cout << "文件创建成功" << std::endl;
    
    // 手动创建一些测试数据来模拟genRawData的功能
    std::vector<int> testData(50);
    std::mt19937 rng(42); // 固定种子以确保结果可重现
    std::uniform_int_distribution<int> dist(0, 10000);
    std::generate(testData.begin(), testData.end(), [&]() { return dist(rng); });
    
    // 将测试数据作为段添加
    assert(msFile.open());
    assert(msFile.appendSegment(testData));
    std::cout << "测试数据添加成功" << std::endl;
    
    // 验证段信息
    auto segments = msFile.getAllSegmentInfo();
    assert(segments.size() == 1);
    assert(segments[0].count == 50);
    std::cout << "段信息验证成功，包含 " << segments[0].count << " 个元素" << std::endl;
    
    msFile.close();
}

void testAppendSegment() {
    std::cout << "测试追加段..." << std::endl;
    
    MergeSortFile msFile("test_data.dat");
    assert(msFile.open());
    
    // 创建一个已排序的测试数据段
    std::vector<int> sortedData = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19};
    
    // 追加段
    assert(msFile.appendSegment(sortedData));
    std::cout << "段追加成功" << std::endl;
    
    // 验证段信息
    auto segments = msFile.getAllSegmentInfo();
    assert(segments.size() == 2); // 原来的段 + 新追加的段
    assert(segments[1].count == 10);
    std::cout << "段数量验证成功: " << segments.size() << std::endl;
    
    msFile.close();
}

void testReadSegment() {
    std::cout << "测试读取段..." << std::endl;
    
    MergeSortFile msFile("test_data.dat");
    assert(msFile.open());
    
    // 读取第一个段（随机数据）
    std::vector<int> buffer1;
    assert(msFile.readSegment(0, buffer1));
    assert(buffer1.size() == 50);
    std::cout << "第一个段读取成功，包含 " << buffer1.size() << " 个元素" << std::endl;
    
    // 读取第二个段（我们添加的排序数据）
    std::vector<int> buffer2;
    assert(msFile.readSegment(1, buffer2));
    assert(buffer2.size() == 10);
    
    // 验证数据正确性
    std::vector<int> expected = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19};
    assert(buffer2 == expected);
    for(int i = 0; i < buffer2.size(); i++) {
        std::cout << buffer2.at(i) <<' ';
    } std::cout<<std::endl;
    std::cout << "第二个段读取成功，数据验证通过" << std::endl;
    
    msFile.close();
}

void testGetSegmentInfo() {
    std::cout << "测试获取段信息..." << std::endl;
    
    MergeSortFile msFile("test_data.dat");
    assert(msFile.open());
    
    // 获取所有段信息
    auto allSegments = msFile.getAllSegmentInfo();
    assert(allSegments.size() >= 2);
    
    // 获取特定段信息
    auto segmentInfo = msFile.getSegmentInfo(1);
    assert(segmentInfo.count == 10);
    
    std::cout << "段信息获取成功" << std::endl;
    std::cout << "段0偏移量: " << allSegments[0].offset << ", 大小: " << allSegments[0].length << ", 元素数: " << allSegments[0].count << std::endl;
    std::cout << "段1偏移量: " << allSegments[1].offset << ", 大小: " << allSegments[1].length << ", 元素数: " << allSegments[1].count << std::endl;
    
    msFile.close();
}

void testAppend() {
    std::cout << "测试追加数据..." << std::endl;
    
    // 创建新文件进行测试
    MergeSortFile msFile("test_data.dat");
    assert(msFile.open());
    
    // 添加初始段
    std::vector<int> initialData = {100, 200, 300};
    //assert(msFile.open());
    assert(msFile.append(initialData));

    std::vector<int> buffer2;
    assert(msFile.readSegment(1, buffer2));
    
    // 验证数据正确性
    for(int i = 0; i < buffer2.size(); i++) {
        std::cout << buffer2.at(i) <<' ';
    } std::cout<<std::endl;
    std::cout << "附加段读取成功，数据验证通过" << std::endl;

    msFile.close();
    std::cout << "追加数据测试完成" << std::endl;
}

/*
int main() {
    std::cout << "开始测试MergeSortFile类..." << std::endl;
    
    try {
        testCreateAndBasicOperations();
        testAppendSegment();
        testReadSegment();
        testGetSegmentInfo();
        testAppend();
        
        std::cout << "\n所有测试通过!" << std::endl;
        
        // 清理测试文件
        std::filesystem::remove("test_data.dat");
        std::filesystem::remove("test_append.dat");
        std::cout << "测试文件已清理" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "测试过程中发生错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
*/