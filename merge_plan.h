#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "run_store.h"

struct MergePlanNode {
    bool is_leaf;
    std::unique_ptr<MergePlanNode> left, right;
    std::string run_fname; // run所在文件
    size_t run_length;
    std::string out_fname; // 输出文件 内部节点才有效
    uint32_t id;

    MergePlanNode(bool leaf, const std::string &file = "", size_t len = 0)
    : is_leaf(leaf), run_fname(file), run_length(len), id(0) {}
};

/* 建立一棵二路归并的归并计划树 */
std::unique_ptr<MergePlanNode>
make_binary_merge_plan(RunStore &store, std::vector<uint32_t> runs);

/* 执行操作计划 */
void excute_merge_plan(MergePlanNode *root,
                      RunStore &in_store,
                      RunStore &out_store);

/* 执行操作计划并返回结果id */
uint32_t execute_merge_plan_return_id(MergePlanNode *root,
                                     RunStore &in_store,
                                     RunStore &out_store);