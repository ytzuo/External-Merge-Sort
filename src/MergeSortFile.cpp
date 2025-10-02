#include "../include/MergeSortFile.h"
#include <string>
#include <fstream>
#include <utility>
#include <filesystem>
#include <vector>
#include <random>
#include <algorithm>
#include <iostream>


/* 仅用于首次灌乱序数据：当作 segment-0 追加 */
bool MergeSortFile::
genRawData(const std::string& fname, int segment_len) {
    if (!create(fname, 1024)) return false;   // blockSize 随便给
    std::vector<int> tmp(segment_len);
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist;
    for (auto& v : tmp) v = dist(rng);
    return appendSegment(tmp);                // 复用追加逻辑
}

/* 创建空文件，只写头 */
bool MergeSortFile::
create(const std::string& fname, int blkSize) {
    filename = fname;
    if(!file.is_open())
        file.open(filename, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file) return false;

    header = FileHeader{};
    header.magic[0] = 'M'; header.magic[1] = 'S';
    header.magic[2] = 'R'; header.magic[3] = 'T';
    header.blockSize = blkSize;
    header.dataStartOffset = sizeof(FileHeader); // 此时 numSegments=0
    writePOD(file, &header, sizeof(header));
    file.close();
    segments.clear();
    return true;
}

/* 打开已有文件，把 header + 所有 meta 读进内存 */
bool MergeSortFile::
open() {
    if(!file.is_open()) {
        //std::cout<<"打开"<<filename<<std::endl;
        file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        //std::cout<<"打开"<<filename<<"完成"<<std::endl;
    }
    if (!file.is_open()) return false;
    readPOD(file, &header, sizeof(header));
    //std::cout<<"检查魔数"<<std::endl;
    if (std::string(header.magic, 4) != "MSRT") return false;
    //std::cout<<"检查魔数通过"<<std::endl;
    segments.resize(header.numSegments);
    if (header.numSegments > 0) {
        file.seekg(sizeof(FileHeader));
        readPOD(file, segments.data(), segments.size() * sizeof(SegmentMetadata));
    }
    return true;
}

bool MergeSortFile::
is_open() {
    return file.is_open();
}

void MergeSortFile::
close() {
    if(file.is_open()) {
        file.close();
    }
}

static std::vector<int> makeRawRun(int len) {
    static std::mt19937_64 rng(42);
    std::vector<int> ret(len);

    std::uniform_int_distribution<int> dist(0, 10000);
    std::generate(ret.begin(), ret.end(), [&] {return dist(rng); });
    return ret;
}

/* 通用追加：把 sortedData 作为新 run 写到 EOF，再追加 meta */
bool MergeSortFile::
appendSegment(const std::vector<int>& sortedRun) {
    if (!file.is_open() && !open()) return false;

    size_t nBytes = sortedRun.size() * sizeof(int);
    /* 1. 数据 → EOF */
    file.seekp(0, std::ios::end);
    size_t runOffset = file.tellp();
    writePOD(file, sortedRun.data(), nBytes);

    /* 2. meta → 当前 metaTail */
    SegmentMetadata meta{ runOffset, nBytes, static_cast<int>(sortedRun.size()), true };
    size_t metaPos = sizeof(FileHeader) + header.numSegments * sizeof(SegmentMetadata);
    file.seekp(metaPos);
    writePOD(file, &meta, sizeof(meta));

    /* 3. 更新 header */
    header.numSegments++;
    file.seekp(0);
    writePOD(file, &header, sizeof(header));

    segments.push_back(meta);
    return true;
}

bool MergeSortFile::
append(const std::vector<int>& sortedData) {
    if (!file.is_open() && !open()) return false;
    
    // 确保至少有一个段存在
    if (segments.empty() || header.numSegments == 0) return false;

    size_t nBytes = sortedData.size() * sizeof(int);
    
    /* 数据 → EOF */
    file.seekp(0, std::ios::end);
    writePOD(file, sortedData.data(), nBytes);
    
    /* 更新最后一个段的元数据 */
    SegmentMetadata& lastSegment = segments.back();
    lastSegment.length += nBytes;
    lastSegment.count += static_cast<int>(sortedData.size());
    
    // 更新文件中存储的段元数据
    size_t metaPos = sizeof(FileHeader) + (header.numSegments - 1) * sizeof(SegmentMetadata);
    file.seekp(metaPos);
    writePOD(file, &lastSegment, sizeof(lastSegment));

    return true;
}

/* 读取指定 run 到 buffer */
bool MergeSortFile::
readSegment(int segId, std::vector<int>& buffer) {
    if (segId < 0 || segId >= header.numSegments) return false;
    const auto& m = segments[segId];
    buffer.resize(m.count);
    file.seekg(m.offset);
    readPOD(file, buffer.data(), m.length);
    return true;
}

/* 流式读取指定run的一部分数据 */
bool MergeSortFile::
readSegmentChunk(int segId, size_t startOffset, size_t count, std::vector<int>& buffer) {
    if (segId < 0 || segId >= header.numSegments) return false;
    
    const auto& m = segments[segId];
    
    // 检查边界
    if (startOffset >= static_cast<size_t>(m.count) || 
        startOffset + count > static_cast<size_t>(m.count)) {
        return false;
    }
    
    // 调整buffer大小
    buffer.resize(count);
    
    // 计算文件中的实际偏移量
    size_t fileOffset = m.offset + startOffset * sizeof(int);
    
    // 定位并读取数据
    file.seekg(fileOffset);
    readPOD(file, buffer.data(), count * sizeof(int));
    
    return true;
}


SegmentMetadata MergeSortFile::
getSegmentInfo(int segId) const {
        return segments.at(segId);
}

std::vector<SegmentMetadata> MergeSortFile::
getAllSegmentInfo() const {
    return segments;    
}