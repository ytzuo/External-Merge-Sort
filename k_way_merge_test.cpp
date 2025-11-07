// TODO : 实现k路归并测试代码
#include <iostream>
#include "k_way_merge_threads.h"
#include "threads.h"
#include <random>
#include <vector>

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


void k_way_merge_test() {
    const std::string RAW_FILENAME  = "raw.runs";
    const std::string INIT_FILENAME =  "initial.runs";
    const std::string MERGED_FILENAME = "merged.runs";
    const uint64_t TOTAL = 1000000;
    const int K = 4;

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

    std::vector<std::vector<int>> merge_tasks = generate_task(K, RUN_NUM);
    // 待完成: K路归并的三线程的初始化和运行
}

int main() {
    // int K = 4;
    // int total_runs = 9;
    // std::vector<std::vector<int>> tasks;
    // tasks = generate_task(K, total_runs);
    // for(std::vector t: tasks) {
    //     for(int i = 0; i < t.size(); i++) {
    //         std::cout << t[i] << " ";
    //     }
    //     std::cout<<std::endl;
    // }
    k_way_merge_test();
    return 0;
}
