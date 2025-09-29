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
    file.open(filename, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file) return false;

    header = FileHeader{};
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
    file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) return false;
    readPOD(file, &header, sizeof(header));
    if (std::string(header.magic, 4) != "MSRT") return false;
    segments.resize(header.numSegments);
    if (header.numSegments > 0) {
        file.seekg(sizeof(FileHeader));
        readPOD(file, segments.data(), segments.size() * sizeof(SegmentMetadata));
    }
    return true;
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

SegmentMetadata MergeSortFile::
getSegmentInfo(int segId) const {
        return segments.at(segId);
}

std::vector<SegmentMetadata> MergeSortFile::
getAllSegmentInfo() const {
    return segments;    
}