#include "../include/MergeSortFile.h"
#include <cstddef>
#include <string>
#include <fstream>
#include <vector>
#include <random>
#include <algorithm>
#include <iostream>


/* 仅用于首次灌乱序数据：当作 segment-0 追加 */
bool MergeSortFile::
genRawData(const std::string& fname, int segment_len, size_t blkSize) {
    if (!create(fname, blkSize, false)) return false;   // blockSize 随便给
    std::vector<int> tmp(segment_len);
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 10000);  // 限制随机数范围在0-10000之间
    for (auto& v : tmp) v = dist(rng);
    return appendSegment(tmp);                // 复用追加逻辑
}

/* 创建空文件，只写头 */
bool MergeSortFile::
create(const std::string& fname, int blkSize, bool init=false) {
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
    
    // 留出足够的空间存储至少100个段的元数据
    const size_t reservedMetaSpace = 100 * sizeof(SegmentMetadata);
    if(init) {
        SegmentMetadata meta{ sizeof(FileHeader)+reservedMetaSpace, 0, 0, false};
        size_t metaPos = sizeof(FileHeader);
        file.seekp(metaPos);
        writePOD(file, &meta, sizeof(meta));
        // 更新header中的段数量
        header.numSegments = 1;
        file.seekp(0);
        writePOD(file, &header, sizeof(header));
        // 预留空间用于存储段元数据，避免与数据区域重叠
        file.seekp(sizeof(FileHeader) + reservedMetaSpace-sizeof(meta));
        writePOD(file, static_cast<const void*>(&header), sizeof(header)); // 写入一个空字节作为占位符
    } else {
        // 预留空间用于存储段元数据，避免与数据区域重叠
        file.seekp(sizeof(FileHeader) + reservedMetaSpace);
        writePOD(file, static_cast<const void*>(&header), sizeof(header)); // 写入一个空字节作为占位符
    }
    
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
    file.seekg(0);
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
SegmentMetadata MergeSortFile::
getSegmentInfo(int segId) const {
    // 创建一个非const副本以重新读取元数据
    MergeSortFile* self = const_cast<MergeSortFile*>(this);
    
    // 确保文件已打开
    if (!self->file.is_open()) {
        self->file.open(self->filename, std::ios::in | std::ios::out | std::ios::binary);
    }
    
    // 重新读取指定段的元数据
    self->file.seekg(sizeof(FileHeader) + segId * sizeof(SegmentMetadata));
    SegmentMetadata meta;
    readPOD(self->file, &meta, sizeof(meta));
    
    return meta;
}

std::vector<SegmentMetadata> MergeSortFile::
getAllSegmentInfo() const {
    // 创建一个非const副本以重新读取元数据
    MergeSortFile* self = const_cast<MergeSortFile*>(this);
    
    // 重新打开文件以确保能读取最新数据
    if (!self->file.is_open()) {
        self->file.open(self->filename, std::ios::in | std::ios::out | std::ios::binary);
    }
    
    // 重新读取头部信息
    self->file.seekg(0);
    readPOD(self->file, &self->header, sizeof(self->header));
    
    // 重新读取所有段元数据
    self->segments.resize(self->header.numSegments);
    if (self->header.numSegments > 0) {
        self->file.seekg(sizeof(FileHeader));
        readPOD(self->file, self->segments.data(), self->segments.size() * sizeof(SegmentMetadata));
    }
    
    return self->segments;
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
    //std::cout<<"runOffset: "<<runOffset<<std::endl;
    writePOD(file, sortedRun.data(), nBytes);

    /* 2. meta → 当前 metaTail */
    SegmentMetadata meta{ runOffset, nBytes, static_cast<int>(sortedRun.size()), true };
    size_t metaPos = sizeof(FileHeader) + header.numSegments * sizeof(SegmentMetadata);
    //std::cout<<"meta.conut: "<<meta.count<<std::endl;
    file.seekp(metaPos);
    writePOD(file, &meta, sizeof(meta));

    /* 3. 更新 header */
    header.numSegments++;
    file.seekp(0);
    writePOD(file, &header, sizeof(header));

    segments.push_back(meta);
    
    // 强制将更改刷新到磁盘
    file.flush();
    
    return true;
}
bool MergeSortFile::
append(const std::vector<int>& sortedData) {
    std::cout<<"append()!"<<std::endl;
    if (!file.is_open() && !open()) return false;
    
    // 确保至少有一个段存在
    if (segments.empty() || header.numSegments == 0) return false;
    std::cout<<"IN append() numSegments "<<header.numSegments<<std::endl;

    size_t nBytes = sortedData.size() * sizeof(int);
    
    /* 数据 → EOF */
    file.seekp(0, std::ios::end);
    writePOD(file, sortedData.data(), nBytes);
    
    /* 更新最后一个段的元数据 */
    SegmentMetadata& lastSegment = segments.back();
    lastSegment.length += nBytes;
    lastSegment.count += static_cast<int>(sortedData.size());
    std::cout<<filename<<" lastSegment.count: "<<lastSegment.count<<std::endl;
    
    // 更新文件中存储的段元数据
    size_t metaPos = sizeof(FileHeader) + (header.numSegments - 1) * sizeof(SegmentMetadata);
    file.seekp(metaPos);
    writePOD(file, &lastSegment, sizeof(lastSegment));
    
    // 强制将更改刷新到磁盘
    file.flush();

    return true;
}

/* 读取指定 run 到 buffer */
bool MergeSortFile::
readSegment(int segId, std::vector<int>& buffer) {
    //std::cout<<"header.numSegments: "<<header.numSegments<<std::endl;
    if (segId < 0 || segId >= header.numSegments) return false;
    
    // 重新加载元数据以确保获取最新信息
    file.seekg(sizeof(FileHeader) + segId * sizeof(SegmentMetadata));
    SegmentMetadata m;
    readPOD(file, &m, sizeof(m));
    
    std::cout<<filename<<" m.count: "<<m.count<<std::endl;
    buffer.resize(m.count);
    file.seekg(m.offset);
    readPOD(file, buffer.data(), m.length);
    return true;
}

/* 流式读取指定run的一部分数据 */
bool MergeSortFile::
readSegmentChunk(int segId, size_t startOffset, size_t count, std::vector<int>& buffer) {
    //std::cout<<"header.numSegments: "<<header.numSegments<<std::endl;
    if (segId < 0 || segId >= header.numSegments) return false;
    
    const auto& m = segments[segId];
    //std::cout<<"m.count: "<<m.count<<std::endl;
    
    // 检查边界
    if (startOffset >= static_cast<size_t>(m.count)) {
        //std::cout<<startOffset<<" "<<static_cast<size_t>(m.count)<<std::endl;
        std::cout<<"startOffset >= static_cast<size_t>(m.count)"<<std::endl;
        return false;
    }
    //如果读取到超出尾部, 减少读取量为正好到尾部
    if (startOffset + count > static_cast<size_t>(m.count)) {
        count = m.count - startOffset;
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

bool MergeSortFile::
writeHeader(size_t block_size) {
    if(!file.is_open()) return false;
    FileHeader header;
    header.blockSize = block_size;
    header.dataStartOffset = sizeof(header);
    header.magic[0] = 'M'; header.magic[1] = 'S';
    header.magic[2] = 'R'; header.magic[3] = 'T';
    header.version = 1;
    header.numSegments = 0;
    file.seekp(0, std::ios::beg);
    writePOD(file, static_cast<const void *>(&header), sizeof(header));
    return true;
}