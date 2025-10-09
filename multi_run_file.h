#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

struct MappedRange {               
    const uint8_t *data = nullptr;
    uint64_t       bytes = 0;

    class Impl;                    // Pimpl设计模式 隐藏 std::span 或内存块生命期
    Impl *impl_ = nullptr;
    MappedRange() = default;
    MappedRange(const uint8_t *d, uint64_t b, Impl *i);
    ~MappedRange();
    MappedRange(MappedRange&&) noexcept;
    MappedRange& operator=(MappedRange&&) noexcept;
    // 禁止拷贝，防止悬针
    MappedRange(const MappedRange&) = delete;
    MappedRange& operator=(const MappedRange&) = delete;
};

class MultiRunFile {
public:
    // new_file=true 时创建空文件
    MultiRunFile(const std::string &path, bool new_file,
                 uint32_t block_size = 1 << 12); // 4096
    ~MultiRunFile();

    void append_run(const int64_t *keys, uint64_t n);   // 追加一个 run
    MappedRange map_run(uint32_t run_id) const;         // 只读映射第 run_id 个 run
    uint32_t run_count() const { return header_.run_count; }

private:
    /* 文件头和元数据信息 */
    struct Header {             
        uint32_t block_sz;           // 块大小
        uint32_t run_count;          // run 的个数   
        uint64_t directory_offset;   // 文件中的绝对偏移
    } header_;

    /* directory 提供每个 run 的位置和大小信息 */
    struct RunEntry {           
        uint64_t offset;         // run 的偏移
        uint64_t keys;           // 数据元素的数量
        uint64_t bytes;          // 长度
    };

    std::string               path_;
    mutable std::fstream      file_;    // 读写同一句柄，mutable 为了 lazy map
    std::vector<RunEntry>     directory_;
    uint64_t                  write_offset_; // 当前文件尾

    void flush_directory();      // 把 directory_ 写到文件尾部并更新 header
};
