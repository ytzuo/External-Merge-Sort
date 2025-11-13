#include "merge_plan.h"
#include "run_store.h"
#include "buffer.h"
#include <queue>
#include <vector>
#include <iostream>

static constexpr size_t MEM_BUF = 1 << 20; // 8MB 内存缓冲

static uint32_t two_way_merge(RunStore &store,
                          uint32_t id1, uint32_t id2,
                          RunStore &out_store) 
{
    //std::cout << "开始两路归并: run_id1=" << id1 << ", run_id2=" << id2 << std::endl;
    
    InputBuffer buf1(store, id1);
    InputBuffer buf2(store, id2);
    OutputBuffer out_buf(out_store);
    out_store.begin_run();
    /* 归并两个已排序序列 */
    while (buf1.has_next() && buf2.has_next()) {
        /* 取出数据但是不移动指针 */
        int64_t val1 = buf1.peek();
        int64_t val2 = buf2.peek();
        if (val1 <= val2) {
            out_buf.add(buf1.next());
        } else {
            out_buf.add(buf2.next());
        }
    }
    
    /* 处理剩余元素 */
    while (buf1.has_next()) {
        out_buf.add(buf1.next());
    }
    while (buf2.has_next()) {
        out_buf.add(buf2.next());
    }

    //std::cout << "完成两路归并: 生成新的run_id=" << (out_store.run_count() - 1) << std::endl;
    return out_store.run_count()-1;
}

std::unique_ptr<MergePlanNode>
make_binary_merge_plan(RunStore &store,
                       std::vector<uint32_t> runs) {
    if(runs.empty()) return nullptr;

    //std::cout << "构建二路归并计划树，输入runs: ";
    // for (auto id : runs) {
    //     std::cout << id << " ";
    // }
    // std::cout << std::endl;

    /* 首先为每个 run 构造叶子节点 */
    std::vector<std::unique_ptr<MergePlanNode>> trees;
    for(auto id : runs) {
        auto[p, n] = store.get_run(id);
        auto node = std::make_unique<MergePlanNode>(true, store.path(), n);
        node->id = id;  // 设置节点的id
        trees.push_back(std::move(node));
        //std::cout << "创建叶子节点: id=" << id << ", length=" << n << std::endl;
    }

    auto cmp = [](const auto& a, const auto& b){
        return a->run_length > b->run_length;
    };
    /* 使用类似哈夫曼树的思想: 每次都将最短的两个进行归并 */
    std::priority_queue<std::unique_ptr<MergePlanNode>,
                        std::vector<std::unique_ptr<MergePlanNode>>,
                        decltype(cmp)> pq(cmp);
    for(auto &t : trees) 
        pq.push(std::move(t));
    while(pq.size() > 1) {
        auto l = std::move(const_cast<std::unique_ptr<MergePlanNode>&>(pq.top()));
        pq.pop();
        auto r = std::move(const_cast<std::unique_ptr<MergePlanNode>&>(pq.top()));
        pq.pop();
        auto parent = std::make_unique<MergePlanNode>(false);
        parent->left = std::move(l);
        parent->right = std::move(r);
        parent->run_length =
            parent->left->run_length + parent->right->run_length;
        //std::cout << "创建内部节点: left_len=" << parent->left->run_length
                  //<< ", right_len=" << parent->right->run_length
                  //<< ", combined_len=" << parent->run_length << std::endl;
        pq.push(std::move(parent));
    }
    
    //std::cout << "二路归并计划树构建完成" << std::endl;
    return pq.empty() ? nullptr : std::move(const_cast<std::unique_ptr<MergePlanNode>&>(pq.top()));
}

// 执行归并计划并返回结果run的id，不修改节点
uint32_t execute_merge_plan_return_id(MergePlanNode *root,
                                      RunStore &in_store,
                                      RunStore &out_store)
{
    if(root == nullptr) {
        //std::cout << "归并计划树为空，无需执行" << std::endl;
        return UINT32_MAX; // 无效id
    }
    if(root->is_leaf) {
        //std::cout << "叶子节点，id=" << root->id << "，无需归并" << std::endl;
        return root->id;
    }
    
    //std::cout << "执行归并计划节点: left_id=" << root->left->id 
              //<< ", right_id=" << root->right->id << std::endl;
    
    // 递归执行子节点并获取结果id
    uint32_t id1 = execute_merge_plan_return_id(root->left.get() , in_store, out_store);
    uint32_t id2 = execute_merge_plan_return_id(root->right.get(), in_store, out_store);

    // 执行当前节点的归并操作
    // 所有归并操作都从out_store读取输入和输出到out_store的最后
    uint32_t result_id = two_way_merge(out_store, id1, id2, out_store);
    //out_store.run_count()-1; // 新生成的run id
    
    //auto [p, n] = out_store.get_run(result_id);
    //std::cout << "节点执行完成: left_id=" << id1 << ", right_id=" << id2 
              //<< ", result_id=" << result_id << std::endl;
              
    return result_id;
}