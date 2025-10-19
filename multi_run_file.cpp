#include "multi_run_file.h"
#include <cstdint>
#include <fstream>
#include <cstring>
#include <ios>
#include <stdexcept>
#include <sstream>
#include <iostream>

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
    if(new_file) {
        file_.open(path, std::ios::binary | std::ios::out | std::ios::trunc | std::ios::in);
        chk(file_.is_open(), "open for write");
        header_.block_sz         = block_size;
        header_.run_count        = 0;
        header_.directory_offset = 0;
        file_.write(reinterpret_cast<const char*>(&header_), sizeof(header_));
        chk(file_.good(), "write header");
        write_offset_ = sizeof(header_);
    } else {
        file_.open(path, std::ios::binary | std::ios::in | std::ios::out);
        chk(file_.is_open(), "open for read");
        file_.read(reinterpret_cast<char*>(&header_), sizeof(header_));
        chk(file_.good() && file_.gcount() == sizeof(header_), "read header");
        directory_.resize(header_.run_count);
        file_.seekg(header_.directory_offset);
        file_.read(reinterpret_cast<char*>(directory_.data()), sizeof(RunEntry) * header_.run_count);
        chk(file_.good(), "read directories");
        // 只考虑数据区末尾，不考虑目录区
        if (!directory_.empty()) {
            const RunEntry& last_entry = directory_.back();
            write_offset_ = last_entry.offset + ((last_entry.bytes + header_.block_sz - 1) & ~(uint64_t(header_.block_sz) - 1));
        } else {
            write_offset_ = sizeof(header_);
        }
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

    directory_.push_back({write_offset_, n, bytes});  // 使用实际字节数而不是对齐后的字节数
    write_offset_ += aligned;
    header_.run_count++;
    
    // 立即更新目录信息到文件，确保数据一致性
    //flush_directory();
}

void MultiRunFile::begin_run() {
    // 创建一个新的空run
    std::vector<int64_t> empty;
    append_run(empty.data(), 0);
    in_run_ = true;
    //header_.run_count++;
    current_run_ = header_.run_count-1;
}

void MultiRunFile::append_to_run(const int64_t *keys, uint64_t n) {
    if (!in_run_) {
        throw std::runtime_error("Not in a run");
    }
    
    // 获取当前run的信息
    RunEntry& entry = directory_[current_run_];
    // 计算新数据的字节大小
    const uint64_t new_bytes = n * sizeof(int64_t);
    const uint64_t old_bytes = entry.bytes;
    const uint64_t total_bytes = old_bytes + new_bytes;
    const uint64_t total_keys = entry.keys + n;

    // 计算按块对齐的占用，只有当该 run 在数据区末尾时才需扩大 write_offset_
    auto align_up = [&](uint64_t b)->uint64_t {
        return (b + header_.block_sz - 1) & ~(uint64_t(header_.block_sz) - 1);
    };
    const uint64_t old_aligned = align_up(old_bytes);
    const uint64_t new_aligned = align_up(total_bytes);

    // 在当前run末尾追加数据
    file_.seekp(entry.offset + old_bytes);
    file_.write(reinterpret_cast<const char*>(keys), new_bytes);
    chk(file_.good(), "write data in append_to_run");

    // 如果这个 run 的存储区域紧贴文件尾（即可以扩展），则我们可能需要填充并调整 write_offset_
    if (entry.offset + old_aligned == write_offset_) {
        if (new_aligned > old_aligned) {
            // 先写入必要的 0 pad 以清除可能残留的元数据
            uint64_t pad_bytes = new_aligned - total_bytes;
            if (pad_bytes > 0) {
                std::vector<uint8_t> zero(pad_bytes, 0);
                file_.write(reinterpret_cast<const char*>(zero.data()), zero.size());
                chk(file_.good(), "pad zero in append_to_run");
            }
            write_offset_ += (new_aligned - old_aligned);
        }
    } else {
        // 如果不是在文件尾，且new_aligned > old_aligned，说明覆盖或超出预分配区域，抛出错误
        if (new_aligned > old_aligned) {
            std::ostringstream oss;
            oss << "append_to_run would exceed allocated region for run " << current_run_
                << " (offset=" << entry.offset << ", old_aligned=" << old_aligned
                << ", new_aligned=" << new_aligned << ")";
            throw std::runtime_error(oss.str());
        }
    }

    // 更新entry信息（仅在内存中，不写入文件）
    entry.bytes = total_bytes;
    entry.keys = total_keys;
    directory_[current_run_] = entry;
}

void MultiRunFile::end_run() {
    in_run_ = false;
    header_.run_count = directory_.size();
    //current_run_++;
    // 可能需要刷新目录信息
    //flush_directory();
}

void MultiRunFile::flush_directory() {
    // 目录区始终在所有数据区之后
    header_.directory_offset = write_offset_;
    file_.seekp(0);
    file_.write(reinterpret_cast<const char*>(&header_), sizeof(header_));
    chk(file_.good(), "rewrite header");
    file_.seekp(header_.directory_offset);
    file_.write(reinterpret_cast<const char*>(directory_.data()), directory_.size() * sizeof(RunEntry));
    chk(file_.good(), "write directory");
    // 不更新write_offset_，write_offset_始终指向数据区末尾
}

void MultiRunFile::reload_directory() const {
    if (header_.run_count > 0) {
        // 重新读取目录信息，确保获取最新的offset值
        file_.seekg(header_.directory_offset);
        file_.read(reinterpret_cast<char*>(const_cast<RunEntry*>(directory_.data())), 
                   sizeof(RunEntry) * header_.run_count);
    }
}

MappedRange MultiRunFile::map_run(uint32_t run_id) const {
    if (!(run_id < header_.run_count)) {
        std::ostringstream oss;
        oss << "run_id " << run_id << " out of range (max " << header_.run_count - 1 << ')';
        throw std::runtime_error(oss.str());
    }
    const RunEntry &e = directory_[run_id]; // 获取指定元数据

    /* 创建新的 Impl 来存储整个 run */
    auto impl = new MappedRange::Impl;
    impl->buffer.resize(e.bytes);
    file_.seekg(e.offset);
    file_.read(reinterpret_cast<char*>(impl->buffer.data()), e.bytes);
    if (!file_.good()) {
        std::ostringstream oss;
        oss << "read run data failed. run_id: " << run_id 
            << ", offset: " << e.offset 
            << ", bytes: " << e.bytes;
        throw std::runtime_error(oss.str());
    }
    
    /* 创建并返回新的 MappedRange */
    return { impl->buffer.data(), e.bytes, impl };
}

MappedRange MultiRunFile::map_run_range(uint32_t run_id, uint64_t offset, uint64_t count) const {
    // 重新加载目录确保使用最新的offset信息
    //reload_directory();
    
    //std::cout<<"map_run_range..."<<std::endl;
    if (!(run_id < header_.run_count)) {
        std::ostringstream oss;
        oss << "run_id " << run_id << " out of range (max " << header_.run_count - 1 << ')';
        throw std::runtime_error(oss.str());
    }
    const RunEntry &e = directory_[run_id]; // 获取指定元数据
    
    // 检查边界
    if (offset >= e.keys) {
        std::ostringstream oss;
        oss << "offset " << offset << " out of range (max " << e.keys - 1 << ')';
        throw std::runtime_error(oss.str());
    }
    
    if (offset + count > e.keys) {
        std::ostringstream oss;
        oss << "offset+count " << (offset + count) << " out of range (e.keys " << e.keys << ')';
        throw std::runtime_error(oss.str());
    }
    
    // 如果count为0，直接返回空的范围
    if (count == 0) {
        auto impl = new MappedRange::Impl;
        return { nullptr, 0, impl };
    }
    
    const uint64_t byte_offset = offset * sizeof(int64_t);
    const uint64_t byte_count  = count  * sizeof(int64_t);
    
    /* 创建新的 Impl 来存储 run 的一部分 */
    auto impl = new MappedRange::Impl;
    impl->buffer.resize(byte_count);
    file_.seekg(e.offset + byte_offset);
    file_.read(reinterpret_cast<char*>(impl->buffer.data()), byte_count);
    if (!file_.good()) {
        std::ostringstream oss;
        oss << "read partial run data failed. run_id: " << run_id 
            << ", offset: " << (e.offset + byte_offset)
            << ", bytes: " << byte_count;
        throw std::runtime_error(oss.str());
    }
    
    /* 创建并返回新的 MappedRange */
    //std::cout<<"finish map_run_range..."<<std::endl;
    return { impl->buffer.data(), byte_count, impl };
}

uint64_t MultiRunFile::get_run_size(uint32_t run_id) const {
    if (run_id >= header_.run_count) {
        std::ostringstream oss;
        oss << "run_id " << run_id << " out of range (max " << header_.run_count - 1 << ')';
        throw std::runtime_error(oss.str());
    }
    return directory_[run_id].keys;
}

void MultiRunFile::get_run_metadata(uint32_t id, uint64_t &offset, uint64_t &keys, uint64_t &bytes) const {
    if (id >= header_.run_count) {
        std::ostringstream oss;
        oss << "run_id " << id << " out of range (max " << header_.run_count - 1 << ')';
        throw std::runtime_error(oss.str());
    }
    const RunEntry &e = directory_[id];
    offset = e.offset;
    keys = e.keys;
    bytes = e.bytes;
}