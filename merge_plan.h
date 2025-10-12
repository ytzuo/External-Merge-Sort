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
    std::string run_fname; // run所在文件 叶子节点才有效
    size_t run_length;
    std::string out_fname; // 输出文件 内部节点才有效
    uint32_t id;

    //MergePlanNode(bool leaf, const uint32_t i, const std::string &file = "", size_t len = 0)
    //: is_leaf(leaf), run_fname(file), run_length(len), id(i) {}

    MergePlanNode(bool leaf, const std::string &file = "", size_t len = 0)
    : is_leaf(leaf), run_fname(file), run_length(len) {}
};

/* 建立一棵二路归并的归并计划树 */
std::unique_ptr<MergePlanNode>
make_binary_merge_plan(std::vector<std::string> runs);

/* 执行操作计划 */
void excute_merge_plan(MergePlanNode *root,
                      RunStore &in_store,
                      RunStore &out_store);