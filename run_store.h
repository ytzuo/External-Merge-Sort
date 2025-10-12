#pragma once
#include "multi_run_file.h"
#include <memory>

/* 操作文件类 */
class RunStore {
public:
    explicit RunStore(const std::string &path, bool new_file = false, 
        uint32_t block_size = 1 << 22) // 等价于2^22
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