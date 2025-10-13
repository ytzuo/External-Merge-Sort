#include "merge_plan.h"
#include "run_store.h"
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

void testMerge() {
    const uint64_t TOTAL = 100000; // 十万个
    const uint64_t RUN_SIZE = TOTAL / 10; // 一万个
    const std::string INITIAL = "initial.runs";
    const std::string MERGED   = "merged.runs";

    std::cout << "开始测试归并过程..." << std::endl;
    std::cout << "TOTAL=" << TOTAL << ", RUN_SIZE=" << RUN_SIZE << std::endl;

    /* 生成初始归并段 */
    RunStore in_store(INITIAL, true);
    std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<int64_t> dist(0, 1'000'000'000);
    auto t0 = std::chrono::steady_clock::now();
    for (uint64_t left = TOTAL; left > 0; ) {
        uint64_t cur = std::min(left, RUN_SIZE);
        std::vector<int64_t> buf(cur);
        for (auto &x : buf) x = dist(rng);
        std::sort(buf.begin(), buf.end());
        in_store.add_run(buf);
        left -= cur;
        std::cout << "生成初始run: 当前大小=" << cur << ", 剩余=" << left << std::endl;
    }
    auto t1 = std::chrono::steady_clock::now();

    /* 构建归并计划树 */
    std::vector<uint32_t> runs;
    for(uint32_t i = 0; i < 10; i++) {
        runs.push_back(i);
    }
    std::cout << "构建归并计划树..." << std::endl;
    auto plan = make_binary_merge_plan(in_store, runs);
    auto t2 = std::chrono::steady_clock::now();

    /* 执行外排序 */
    std::cout << "执行外排序..." << std::endl;
    RunStore out_store(MERGED, true);
    excute_merge_plan(plan.get(), in_store, out_store);
    auto t3 = std::chrono::steady_clock::now();

    /* 校验结果 */
    std::cout << "校验结果..." << std::endl;
    // 打印输出存储中的run数量
    std::cout << "输出存储中的run数量: " << out_store.run_count() << std::endl;
    
    // 获取最后一个run作为最终结果
    uint32_t final_run_id = out_store.run_count() - 1;
    std::cout << "使用run id: " << final_run_id << std::endl;
    
    auto [final_p, final_n] = out_store.get_run(final_run_id);
    std::cout << "获取到的元素数量: " << final_n << std::endl;
    
    // 打印前几个和后几个元素用于调试
    if (final_n > 0) {
        std::cout << "前10个元素: ";
        for (uint64_t i = 0; i < std::min(final_n, uint64_t(10)); i++) {
            std::cout << final_p[i] << " ";
        }
        std::cout << std::endl;
        
        std::cout << "后10个元素: ";
        if (final_n > 10) {
            for (uint64_t i = final_n - 10; i < final_n; i++) {
                std::cout << final_p[i] << " ";
            }
        }
        std::cout << std::endl;
    }
    
    bool ok = std::is_sorted(final_p, final_p + final_n);
    std::cout << "Validation " << (ok ? "PASSED" : "FAILED")
              << ", total records: " << final_n << '\n';
    std::cout << "Generate  : " << (t1 - t0).count() / 1e9 << " s\n"
              << "Merge     : " << (t3 - t2).count() / 1e9 << " s\n";
}

int main() {
    testMerge();
    return 0;
}