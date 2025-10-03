#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>      // memcpy
#include <algorithm>    // fill
#include <random>

struct FileHeader {
    char magic[4];
    int  version   = 1;
    int  blockSize = 0;          // 用户设定
    int  numSegments = 0;
    size_t dataStartOffset = sizeof(FileHeader);   // 初始值
};

struct SegmentMetadata {
    size_t offset = 0;   // 相对文件头 0 的绝对偏移
    size_t length = 0;   // 字节数
    int    count  = 0;
    bool   sorted = false;
};

// 二进制读写辅助函数
static void writePOD(std::fstream& fs, const void* pod, size_t sz) {
    fs.write(reinterpret_cast<const char*>(pod), sz);
}
static void readPOD(std::fstream& fs, void* pod, size_t sz) {
    fs.read(reinterpret_cast<char*>(pod), sz);
}

class MergeSortFile {
public:
    explicit MergeSortFile(){};
    explicit MergeSortFile(const std::string& fname) : filename(fname){
        file.open(fname);
    }
    /* 创建空文件，只写头 */
    bool create(const std::string& fname, int blkSize, bool init);

    /* 打开已有文件，把 header + 所有 meta 读进内存 */
    bool open();

    /* 关闭正在读取的文件 */
    void close();

    /* 判断是否已打开文件 */
    bool is_open();

    /* 仅用于首次生成乱序数据：当作 segment-0 追加 */
    bool genRawData(const std::string& fname, int segment_len, size_t blkSize);

    /* 通用追加：把 sortedData 作为新 run 写到 EOF，再追加 meta */
    bool appendSegment(const std::vector<int>& sortedRun);

    /* 仅用于文件中只有一个 run 时, 向尾部附加数据 */
    bool append(const std::vector<int>& sortedData);

    /* 读取指定 run 到 buffer */
    bool readSegment(int segId, std::vector<int>& buffer);

    /* 获取指定 run 的Meta */
    SegmentMetadata getSegmentInfo(int segId) const ;

    /* 流式读取指定run的一部分数据 */
    bool readSegmentChunk(int segId, size_t startOffset, size_t count, std::vector<int>& buffer);
    
    /* 获取所有 run 的Meta */
    std::vector<SegmentMetadata> getAllSegmentInfo() const;

    bool writeHeader(size_t blkSize);
private:
    std::string filename;
    std::fstream file;
    FileHeader header;
    std::vector<SegmentMetadata> segments;
};
