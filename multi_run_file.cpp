#include "multi_run_file.h"
#include <cstdint>
#include <fstream>
#include <cstring>
#include <ios>
#include <stdexcept>

/* 异常处理辅助函数 */
static void chk(bool cond, const std::string& msg) {
    if(!cond)
        throw std::runtime_error(msg);
}

/* MappedRange 实现 */
struct MappedRange::Impl {
    std::vector<uint8_t> buffer;          // 一次性把整段读进来
};

/* MappedRange 构造函数实现 */
MappedRange::MappedRange(const uint8_t *d, uint64_t b, Impl *i)
    : data(d), bytes(b), impl_(i) {}
MappedRange::~MappedRange() { delete impl_; }
MappedRange::MappedRange(MappedRange&& o) noexcept
    : data(o.data), bytes(o.bytes), impl_(o.impl_) { o.impl_ = nullptr; }
MappedRange& MappedRange::operator=(MappedRange&& o) noexcept {
    if (this != &o) { delete impl_; data=o.data; bytes=o.bytes; impl_=o.impl_; o.impl_=nullptr; }
    return *this;
}

/* MultiRunFile 实现 */

MultiRunFile::MultiRunFile(const std::string &path, bool new_file, 
    uint32_t block_size)
    :path_(path), write_offset_(0){
    if(new_file) { // 新文件, 要写入Header等基本信息
        file_.open(path, std::ios::binary | std::ios::out | std::ios::trunc);
        chk(file_.is_open(), "open for write");

        header_.block_sz         = block_size;
        /* 新文件, 初始化为0 */
        header_.run_count        = 0; 
        header_.directory_offset = 0;
        file_.write(reinterpret_cast<const char*>(&header_), sizeof(header_));
        chk(file_.good(), "write header");
        write_offset_ = sizeof(header_);
    } else { // 已有文件, 要读取header和目录等基本信息
        file_.open(path, std::ios::binary | std::ios::in);
        chk(file_.is_open(), "open for read");

        file_.read(reinterpret_cast<char*>(&header_), sizeof(header_));
        chk(file_.good() && file_.gcount() == sizeof(header_), "read header");

        directory_.resize(header_.run_count); // 预留等于 run 个数的目录的空间
        file_.seekg(header_.directory_offset);
        file_.read(reinterpret_cast<char*>(directory_.data()), 
                   sizeof(RunEntry) * header_.run_count);
        chk(file_.good(), "read directories");
    }
}

MultiRunFile::~MultiRunFile() {
    if (file_.is_open() && !directory_.empty()) {
        try { flush_directory(); } catch (...) {} // 析构里不要抛异常
    }
}

void MultiRunFile::append_run(const int64_t *keys, uint64_t n) {
    const uint64_t bytes = n * sizeof(int64_t);
    const uint64_t aligned =  
        (bytes + header_.block_sz - 1) &  // 这部分确保即使bytes不是块大小的整数倍，也会向上取整到下一个块边界
        ~(uint64_t(header_.block_sz) - 1); // 对块大小-1 进行取反
        /*
            bytes + block_sz - 1 = 100 + 4096 - 1 = 4195  (二进制: 1000001100011)
            block_sz - 1 = 4095                           (二进制: 0111111111111)  
            ~(block_sz - 1) = ~4095                       (二进制: ...1111000000000000)
            最终结果 = 4195 & (~4095) = 4096               (二进制: ...0001000000000000)
        */
    
    file_.seekp(write_offset_);
    file_.write(reinterpret_cast<const char*>(keys), bytes);
    chk(file_.good(), "write data");

    if (aligned > bytes) {
        std::vector<uint8_t> zero(aligned - bytes, 0); // 用0填满不到边界的位置
        file_.write(reinterpret_cast<const char*>(zero.data()), zero.size());
        chk(file_.good(), "pad zero");
    }

    directory_.push_back({write_offset_, n, aligned});
    write_offset_ += aligned;
    header_.run_count++;
}

void MultiRunFile::flush_directory() {
    header_.directory_offset = write_offset_;

    /* 重写 header, 因为 directory 的位置发生变化 */
    file_.seekp(0);
    file_.write(reinterpret_cast<const char*>(&header_), sizeof(header_));
    chk(file_.good(), "rewrite header");
    /* 重写 directory, 因为 run 数量发生变化 */
    file_.seekp(header_.directory_offset);
    file_.write(reinterpret_cast<const char*>(directory_.data()),
                directory_.size() * sizeof(RunEntry));
    chk(file_.good(), "write directory");

}

MappedRange MultiRunFile::map_run(uint32_t run_id) const {
    chk(run_id < header_.run_count, "run_id out of range");
    const RunEntry &e = directory_[run_id]; // 获取指定元数据

    /* 创建新的 Impl 来存储整个 run */
    auto impl = new MappedRange::Impl;
    impl->buffer.resize(e.bytes);
    file_.seekg(e.offset);
    file_.read(reinterpret_cast<char*>(impl->buffer.data()), e.bytes);
    chk(file_.good(), "read run data");
    
    /* 创建并返回新的 MappedRange */
    return { impl->buffer.data(), e.bytes, impl };
}

MappedRange MultiRunFile::map_run_range(uint32_t run_id, uint64_t offset, uint64_t count) const {
    chk(run_id < header_.run_count, "run_id out of range");
    const RunEntry &e = directory_[run_id]; // 获取指定元数据
    
    // 检查边界
    chk(offset < e.keys, "offset out of range");
    chk(offset + count <= e.keys, "count out of range");
    
    const uint64_t byte_offset = offset * sizeof(int64_t);
    const uint64_t byte_count = count * sizeof(int64_t);
    
    /* 创建新的 Impl 来存储 run 的一部分 */
    auto impl = new MappedRange::Impl;
    impl->buffer.resize(byte_count);
    file_.seekg(e.offset + byte_offset);
    file_.read(reinterpret_cast<char*>(impl->buffer.data()), byte_count);
    chk(file_.good(), "read partial run data");
    
    /* 创建并返回新的 MappedRange */
    return { impl->buffer.data(), byte_count, impl };
}