#include "merge_plan.h"
#include "run_store.h"
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

void testMerge() {
    const uint64_t TOTAL = 500000;
    const uint64_t RUN_NUM = 16;
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
        //std::cout << "生成初始run: 当前大小=" << cur << ", 剩余=" << left << std::endl;
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
    
    // 首先将所有初始run复制到输出存储中（使用拥有缓冲区的映射，避免悬空指针）
    for (uint32_t i = 0; i < RUN_NUM; i++) {
        MappedRange m = in_store.map_run_owned(i);
        const int64_t *p = reinterpret_cast<const int64_t*>(m.data);
        uint64_t n = m.bytes / sizeof(int64_t);
        std::vector<int64_t> buf(p, p + n);
        out_store.add_run(buf);
    }
    
    // 使用执行函数
    uint32_t final_result_id = execute_merge_plan_return_id(plan.get(), in_store, out_store);
    auto t3 = std::chrono::steady_clock::now();

    // /* 校验结果 */
    // std::cout << "校验结果..." << std::endl;
    // // 打印输出存储中的run数量
    // std::cout << "输出存储中的run数量: " << out_store.run_count() << std::endl;
    
    // // 检查所有run的内容
    // uint64_t total_elements = 0;
    
    // const uint64_t CHUNK = 65536; // 每次读取的元素数量
    // for (uint32_t i = 0; i < out_store.run_count(); i++) {
    //     uint64_t n = out_store.get_run_size(i);
    //     total_elements += n;
    //     //std::cout << "Run " << i << " 包含 " << n << " 个元素" << std::endl;

    //     if (n == 0) {
    //         //std::cout << "Run " << i << " 排序 正确 (空)" << std::endl;
    //         continue;
    //     }

    //     bool ok = true;
    //     int64_t prev_tail = 0; // 上一块的尾值
    //     bool has_prev_tail = false;
    //     uint64_t offset = 0;
    //     while (offset < n) {
    //         uint64_t cnt = std::min(CHUNK, n - offset);
    //         MappedRange chunk = out_store.map_run_range_owned(i, offset, cnt);
    //         const int64_t *p = reinterpret_cast<const int64_t*>(chunk.data);
    //         uint64_t m = chunk.bytes / sizeof(int64_t);

    //         // 检查本块内部是否有序
    //         for (uint64_t j = 1; j < m; j++) {
    //             if (p[j] < p[j-1]) {
    //                 std::cout << "  Run " << i << " 在块 offset=" << offset << " 内位置 " << j << " 发现未排序: "
    //                           << p[j-1] << " > " << p[j] << std::endl;
    //                 ok = false;
    //                 break;
    //             }
    //         }

    //         // 检查与上一块的边界
    //         if (has_prev_tail && m > 0) {
    //             if (prev_tail > p[0]) {
    //                 std::cout << "  Run " << i << " 边界不连续: prev_tail=" << prev_tail << " > cur_head=" << p[0]
    //                           << " at offset=" << offset << std::endl;
    //                 ok = false;
    //             }
    //         }

    //         // 更新 prev_tail
    //         if (m > 0) {
    //             prev_tail = p[m-1];
    //             has_prev_tail = true;
    //         }

    //         offset += cnt;
    //     }

    //     //std::cout << "Run " << i << " 排序 " << (ok ? "正确" : "错误") << std::endl;
    // }
    
    // std::cout << "总共元素数量: " << total_elements << std::endl;
    
    // std::cout << "使用run id: " << final_result_id << " (最终结果run)" << std::endl;
    
    // // 打印问题 run 的元数据以便诊断
    // for (uint32_t rid : {197u, 198u}) {
    //     if (rid < out_store.run_count()) {
    //         uint64_t off, keys, bytes;
    //         out_store.get_run_metadata(rid, off, keys, bytes);
    //         std::cout << "Run metadata: id=" << rid << ", offset=" << off << ", keys=" << keys << ", bytes=" << bytes << std::endl;
    //     }
    // }

    // auto [_, final_n] = out_store.get_run(final_result_id);
    // std::cout << "获取到的元素数量: " << final_n << std::endl;
    // // 分块读取前后样本，避免一次性分配
    // const uint64_t SAMPLE = 10;
    // if (final_n > 0) {
    //     // 前 SAMPLE
    //     uint64_t front_cnt = std::min(SAMPLE, final_n);
    //     MappedRange front = out_store.map_run_range_owned(final_result_id, 0, front_cnt);
    //     const int64_t *front_p = reinterpret_cast<const int64_t*>(front.data);
    //     std::cout << "前10个元素: ";
    //     for (uint64_t i = 0; i < front_cnt; i++) std::cout << front_p[i] << " ";
    //     std::cout << std::endl;

    //     // 后 SAMPLE
    //     uint64_t back_cnt = std::min(SAMPLE, final_n);
    //     MappedRange back = out_store.map_run_range_owned(final_result_id, final_n - back_cnt, back_cnt);
    //     const int64_t *back_p = reinterpret_cast<const int64_t*>(back.data);
    //     std::cout << "后10个元素: ";
    //     for (uint64_t i = 0; i < back_cnt; i++) std::cout << back_p[i] << " ";
    //     std::cout << std::endl;
    // }

    // 验证最终结果是否包含所有元素（分块校验）
    // bool ok = true;
    // int64_t prev_tail = 0; bool has_prev_tail = false;
    // uint64_t offset = 0;
    // while (offset < final_n) {
    //     uint64_t cnt = std::min(CHUNK, final_n - offset);
    //     MappedRange chunk = out_store.map_run_range_owned(final_result_id, offset, cnt);
    //     const int64_t *p = reinterpret_cast<const int64_t*>(chunk.data);
    //     uint64_t m = chunk.bytes / sizeof(int64_t);
    //     for (uint64_t j = 1; j < m; j++) {
    //         if (p[j] < p[j-1]) { ok = false; break; }
    //     }
    //     if (has_prev_tail && m > 0 && prev_tail > p[0]) { ok = false; }
    //     if (m > 0) { prev_tail = p[m-1]; has_prev_tail = true; }
    //     offset += cnt;
    //     if (!ok) break;
    // }
    // std::cout << "Validation " << (ok ? "PASSED" : "FAILED")
    //           << ", total records: " << final_n << std::endl;
    
    // if (final_n == TOTAL) {
    //     std::cout << "成功: 最终结果包含所有 " << TOTAL << " 个元素" << std::endl;
    // } else {
    //     std::cout << "注意: 最终结果只包含 " << final_n << " 个元素，期望 " << TOTAL << " 个元素" << std::endl;
    //     std::cout << "这可能是因为归并计划树没有完全执行，最终结果可能在另一个run中" << std::endl;
        
    //     // 查找包含最多元素的run
    //     uint64_t max_elements = 0;
    //     uint32_t max_run_id = 0;
    //     for (uint32_t i = 0; i < out_store.run_count(); i++) {
    //         auto [p, n] = out_store.get_run(i);
    //         if (n > max_elements) {
    //             max_elements = n;
    //             max_run_id = i;
    //         }
    //     }
        
    //     if (max_run_id != final_result_id) {
    //         std::cout << "包含最多元素的run是 run_id=" << max_run_id << "，包含 " << max_elements << " 个元素" << std::endl;
    //     }
    // }
              
    std::cout << "Generate  : " << (t1 - t0).count() / 1e9 << " s\n"
              << "Merge     : " << (t3 - t2).count() / 1e9 << " s\n";
}

// int main() {
//     testMerge();
//     return 0;
// }