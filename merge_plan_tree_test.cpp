#include "merge_plan.h"
#include "run_store.h"
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

void testMerge() {
    const uint64_t TOTAL = 10000000;
    const uint64_t RUN_NUM = 100;
    const uint64_t RUN_SIZE = TOTAL / RUN_NUM;
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
    for(uint32_t i = 0; i < RUN_NUM; i++) {
        runs.push_back(i);
    }
    std::cout << "构建归并计划树..." << std::endl;
    auto plan = make_binary_merge_plan(in_store, runs);
    auto t2 = std::chrono::steady_clock::now();

    /* 执行外排序 */
    std::cout << "执行外排序..." << std::endl;
    RunStore out_store(MERGED, true);
    
    // 首先将所有初始run复制到输出存储中
    for (uint32_t i = 0; i < RUN_NUM; i++) {
        auto [p, n] = in_store.get_run(i);
        std::vector<int64_t> buf(p, p + n);
        out_store.add_run(buf);
    }
    
    // 使用新的执行函数
    uint32_t final_result_id = execute_merge_plan_return_id(plan.get(), in_store, out_store);
    auto t3 = std::chrono::steady_clock::now();

    /* 校验结果 */
    std::cout << "校验结果..." << std::endl;
    // 打印输出存储中的run数量
    std::cout << "输出存储中的run数量: " << out_store.run_count() << std::endl;
    
    // 检查所有run的内容
    uint64_t total_elements = 0;
    
    for (uint32_t i = 0; i < out_store.run_count(); i++) {
        auto [p, n] = out_store.get_run(i);
        total_elements += n;
        std::cout << "Run " << i << " 包含 " << n << " 个元素" << std::endl;
        
        // 验证每个run是否已排序
        bool sorted = std::is_sorted(p, p + n);
        std::cout << "Run " << i << " 排序 " << (sorted ? "正确" : "错误") << std::endl;
        
        // 如果排序错误，显示更多详细信息帮助诊断
        if (!sorted && n > 0) {
            std::cout << "  Run " << i << " 前20个元素: ";
            for (uint64_t j = 0; j < std::min(n, uint64_t(20)); j++) {
                std::cout << p[j] << " ";
            }
            std::cout << std::endl;
            
            std::cout << "  Run " << i << " 后20个元素: ";
            if (n > 20) {
                for (uint64_t j = n - std::min(n, uint64_t(20)); j < n; j++) {
                    std::cout << p[j] << " ";
                }
            }
            std::cout << std::endl;
            
            // 查找第一个未排序的位置
            for (uint64_t j = 1; j < n; j++) {
                if (p[j] < p[j-1]) {
                    std::cout << "  Run " << i << " 在位置 " << j << " 处发现未排序元素: " 
                              << p[j-1] << " > " << p[j] << std::endl;
                    break;
                }
            }
        }
    }
    
    std::cout << "总共元素数量: " << total_elements << std::endl;
    
    std::cout << "使用run id: " << final_result_id << " (最终结果run)" << std::endl;
    
    auto [final_p, final_n] = out_store.get_run(final_result_id);
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
    
    // 验证最终结果是否包含所有元素
    bool ok = std::is_sorted(final_p, final_p + final_n);
    std::cout << "Validation " << (ok ? "PASSED" : "FAILED")
              << ", total records: " << final_n << std::endl;
    
    if (final_n == TOTAL) {
        std::cout << "成功: 最终结果包含所有 " << TOTAL << " 个元素" << std::endl;
    } else {
        std::cout << "注意: 最终结果只包含 " << final_n << " 个元素，期望 " << TOTAL << " 个元素" << std::endl;
        std::cout << "这可能是因为归并计划树没有完全执行，最终结果可能在另一个run中" << std::endl;
        
        // 查找包含最多元素的run
        uint64_t max_elements = 0;
        uint32_t max_run_id = 0;
        for (uint32_t i = 0; i < out_store.run_count(); i++) {
            auto [p, n] = out_store.get_run(i);
            if (n > max_elements) {
                max_elements = n;
                max_run_id = i;
            }
        }
        
        if (max_run_id != final_result_id) {
            std::cout << "包含最多元素的run是 run_id=" << max_run_id << "，包含 " << max_elements << " 个元素" << std::endl;
        }
    }
              
    std::cout << "Generate  : " << (t1 - t0).count() / 1e9 << " s\n"
              << "Merge     : " << (t3 - t2).count() / 1e9 << " s\n";
}

int main() {
    testMerge();
    return 0;
}