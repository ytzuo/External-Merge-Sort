#include "buffer.h"
#include "merge_plan.h"
#include "run_store.h"
#include <chrono>
#include <cstdint>
#include "multi_run_file.h"
#include <iostream>
#include <random>
#include <vector>
#include "threads.h"
#include <thread>

void gen_raw_data(const std::string & FILENAME, uint64_t size) {
    // 创建RunStore，使用new_file=true参数创建新文件
    RunStore store(FILENAME, true);
    // 创建随机数生成器
    std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<int64_t> dist(0, 1'000'000'000);
    // 生成乱序数据
    std::vector<int64_t> data(size);
    for (auto &x : data) {
        x = dist(rng);
    }
    // 将乱序数据添加为一个run
    store.add_run(data);
}


void test_multi_thread() {
    const std::string RAW_FILENAME  = "raw.runs";
    const std::string INIT_FILENAME =  "initial.runs";
    const std::string MERGED_FILENAME = "merged.runs";
    const uint64_t TOTAL = 1000000;
    //const uint64_t RUN_NUM = 10

    auto t1 = std::chrono::steady_clock::now();
    std::cout << "生成乱序数据..." << std::endl;
    gen_raw_data(RAW_FILENAME, TOTAL);

    /* 初始化输入缓冲区 */
    RunStore raw_store(RAW_FILENAME);
    std::vector<InputBuffer> ins;
    ins.reserve(2);
    ins.emplace_back(raw_store, 0);
    ins.emplace_back(raw_store, 0);
    /* 初始化输出缓冲区 */
    RunStore init_store(INIT_FILENAME, true);
    std::vector<OutputBuffer> outs;
    outs.reserve(2);
    outs.emplace_back(init_store);
    outs.emplace_back(init_store);

    std::atomic<bool> done_reading(false);
    std::atomic<size_t> next_task(0);
    std::atomic<bool> done_sorting(false);
    std::vector<Task> tasks = initTasks(raw_store);
    /* 启动线程 */
    std::cout << "启动线程..." << std::endl;
    std::thread  read(reader, std::ref(ins), std::ref(done_reading), std::ref(tasks), std::ref(next_task));
    std::thread  sort(sorter, std::ref(ins), std::ref(outs), std::ref(done_reading), std::ref(done_sorting));
    std::thread write(writer, std::ref(outs), std::ref(done_sorting));
    read.join();
    sort.join();
    write.join();
    std::cout << "生成文件元数据..." << std::endl;
    uint32_t RUN_NUM =  init_store.create_entries();
    std::cout<<"生成 RUN_NUM = "<<RUN_NUM<<std::endl;
    auto t2 = std::chrono::steady_clock::now();

    std::vector<uint32_t> runs;
    for(uint32_t i = 0; i < RUN_NUM; i++) {
        runs.push_back(i);
    }
    std::cout << "构建归并计划树..." << std::endl;
    auto plan = make_binary_merge_plan(init_store, runs);
    auto t3 = std::chrono::steady_clock::now();

    /* 执行外排序 */
    std::cout << "执行外排序..." << std::endl;
    RunStore out_store(MERGED_FILENAME, true);
    
    // 首先将所有初始run复制到输出存储中（使用拥有缓冲区的映射，避免悬空指针）
    for (uint32_t i = 0; i < RUN_NUM; i++) {
        MappedRange m = init_store.map_run_owned(i);
        const int64_t *p = reinterpret_cast<const int64_t*>(m.data);
        uint64_t n = m.bytes / sizeof(int64_t);
        std::vector<int64_t> buf(p, p + n);
        out_store.add_run(buf);
    }

     // 使用执行函数
    execute_merge_plan_return_id(plan.get(), init_store, out_store);
    auto t4 = std::chrono::steady_clock::now();
    std::cout << "Generate  : " << (t2 - t1).count() / 1e9 << " s\n"
              << "Merge     : " << (t4 - t3).count() / 1e9 << " s\n";
}


int main() {
    test_multi_thread();
    return 0;
}