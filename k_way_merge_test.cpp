// TODO : 实现k路归并测试代码
#include <iostream>
#include "buffer.h"
#include "buffer_manager.h"
#include "k_way_merge_threads.h"
#include "run_store.h"
#include "threads.h"
#include <ostream>
#include <random>
#include <vector>

static void gen_raw_data(const std::string & FILENAME, uint64_t size) {
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

    auto t3 = std::chrono::steady_clock::now();
    std::vector<std::vector<int>> merge_tasks = generate_task(K, RUN_NUM);
        for(int i = 0; i < merge_tasks.size(); i++) {
        for(int j = 0; j < merge_tasks[i].size(); j++) {
            std::cout<<merge_tasks[i][j]<<" ";
        }
        std::cout<<std::endl;
    }

    std::vector<BufferQueue*> qs;                 // 缓冲区队列, 初始应该为空
    for(int i = 0; i < K; i++) {
        qs.push_back(new BufferQueue());
    }

    std::vector<OutputBuffer*> merge_outs;        // 输出缓冲区, 初始有两个空缓冲区
    RunStore merged_store(MERGED_FILENAME, true);
    // 首先将所有初始run复制到输出存储中（使用拥有缓冲区的映射，避免悬空指针）
    for (uint32_t i = 0; i < RUN_NUM; i++) {
        MappedRange m = init_store.map_run_owned(i);
        const int64_t *p = reinterpret_cast<const int64_t*>(m.data);
        uint64_t n = m.bytes / sizeof(int64_t);
        std::vector<int64_t> buf(p, p + n);
        merged_store.add_run(buf);
    }

    BufferPool pool(2 * K);              // 缓冲池，有 2K 个段
    for(int i = 0; i < 2 * K; i++) {
        pool.returnBuffer(new InputBuffer(merged_store, 0));
    }

    std::vector<InputBuffer*> current_buffers(K, nullptr);

    merge_outs.emplace_back(new OutputBuffer(merged_store));
    merge_outs.emplace_back(new OutputBuffer(merged_store));
    std::vector<int64_t> last_key;                // 最后一个输出的元素
    for(int i = 0; i < K; i++) {
        last_key.push_back(0);
    }
    std::atomic<bool> inited(false);           //  本轮任务是否初始化完成

    InputThread input_thread(K, &merged_store, &pool, qs, last_key, merge_tasks, inited);
    MergeThread merge_thread(K, qs, last_key, merge_outs, &pool, &merged_store, merge_tasks, inited);
    std::cout << "启动K路归并线程..." << std::endl;
    done_sorting.store(false);
    std::thread  input(&InputThread::inputRun, &input_thread, std::ref(current_buffers));
    std::thread  merge(&MergeThread::kWayMerge, &merge_thread, merge_tasks.size(), std::ref(done_sorting), std::ref(current_buffers));
    std::thread output(writer_p, std::ref(merge_outs), std::ref(done_sorting));
    input.join();
    merge.join();
    output.join();
    auto t4 = std::chrono::steady_clock::now();
    std::cout << "Generate  : " << (t2 - t1).count() / 1e9 << " s\n"
              << "Merge     : " << (t4 - t3).count() / 1e9 << " s\n";
}

// int main() {
//     // int K = 4;
//     // int total_runs = 9;
//     // std::vector<std::vector<int>> tasks;
//     // tasks = generate_task(K, total_runs);
//     // for(std::vector t: tasks) {
//     //     for(int i = 0; i < t.size(); i++) {
//     //         std::cout << t[i] << " ";
//     //     }
//     //     std::cout<<std::endl;
//     // }
//     k_way_merge_test();
//     return 0;
// }
