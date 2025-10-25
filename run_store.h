#pragma once
#include "multi_run_file.h"
#include <cstdint>
#include <memory>
#include <iostream>
#include <vector>

/* 操作文件类, 向上层隐藏文件操作的细节 */
class RunStore {
public:
    explicit RunStore(const std::string &path, bool new_file = false, 
        uint32_t block_size = 65536) // 64KB
        : file_(std::make_unique<MultiRunFile>(path, new_file, block_size)){}
    
    
    void begin_run() {
        in_run_ = true;
        file_->begin_run();
    }

    void append_to_run(std::vector<int64_t> keys) {
        file_->append_to_run(keys.data(), keys.size());
    }

    void end_run() {
        in_run_ = false;
        file_->end_run();
    }

    void add_run(const std::vector<int64_t> &keys) {
        file_->append_run(keys.data(), keys.size());
    }

    /* 返回的 run 只读 */
    std::pair<const int64_t*, uint64_t> get_run(const uint32_t id) {
        MappedRange m = file_->map_run(id);
        return {reinterpret_cast<const int64_t*>(m.data),
                m.bytes / sizeof(int64_t)};
    }

    // 返回拥有缓冲区的映射, 调用方需持有返回的 MappedRange 对象以保持数据有效
    MappedRange map_run_owned(const uint32_t id) {
        return file_->map_run(id);
    }

    /* 部分读取 run */
    std::pair<const int64_t*, uint64_t> get_run_range(const uint32_t id, uint64_t offset, uint64_t count) {
        //std::cout<<"get_run_range..."<<std::endl;
        MappedRange m = file_->map_run_range(id, offset, count);
        return {reinterpret_cast<const int64_t*>(m.data),
                (m.bytes / sizeof(int64_t))};
    }

    // 返回拥有缓冲区的部分映射
    MappedRange map_run_range_owned(const uint32_t id, uint64_t offset, uint64_t count) {
        return file_->map_run_range(id, offset, count);
    }

    /* 获取run中元素的数量 */
    uint64_t get_run_size(const uint32_t id) {
        return file_->get_run_size(id);
    }

    void get_run_metadata(const uint32_t id, uint64_t &offset, uint64_t &keys, uint64_t &bytes) {
        file_->get_run_metadata(id, offset, keys, bytes);
    }

    uint32_t run_count() const {
        return file_->run_count();
    }

    std::string path() const {
        return path_;
    }

    void append_direct(std::vector<int64_t> keys) {
        file_->append(keys.data(), keys.size());
    }

    uint32_t create_entries() {        
        return file_->create_entries();
    }

    // MultiRunFile* getFile() {
    //     return file_.get(); // get() 返回内部指针
    // }

private:
    std::unique_ptr<MultiRunFile> file_;
    std::string path_;
    bool in_run_;           // 是否正在一个run中
    uint32_t current_run_;  // 当前run的索引
};