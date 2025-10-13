#pragma once
#include "multi_run_file.h"
#include <memory>

/* 操作文件类, 向上层隐藏文件操作的细节 */
class RunStore {
public:
    explicit RunStore(const std::string &path, bool new_file = false, 
        uint32_t block_size = 1 << 22) // 等价于2^21
        : file_(std::make_unique<MultiRunFile>(path, new_file, block_size)){}
    

    void add_run(const std::vector<int64_t> &keys) {
        file_->append_run(keys.data(), keys.size());
    }

    /* 返回的 run 只读 */
    std::pair<const int64_t*, uint64_t> get_run(const uint32_t id) {
        MappedRange m = file_->map_run(id);
        return {reinterpret_cast<const int64_t*>(m.data),
                m.bytes / sizeof(int64_t)};
    }

    /* 部分读取run */
    std::pair<const int64_t*, uint64_t> get_run_range(const uint32_t id, uint64_t offset, uint64_t count) {
        MappedRange m = file_->map_run_range(id, offset, count);
        return {reinterpret_cast<const int64_t*>(m.data),
                m.bytes / sizeof(int64_t)};
    }

    /* 获取run中元素的数量 */
    uint64_t get_run_size(const uint32_t id) {
        return file_->get_run_size(id);
    }

    uint32_t run_count() const {
        return file_->run_count();
    }

    std::string path() const {
        return path_;
    }
private:
    std::unique_ptr<MultiRunFile> file_;
    std::string path_;
};