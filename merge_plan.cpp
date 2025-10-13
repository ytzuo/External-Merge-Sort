#include "merge_plan.h"
#include "run_store.h"
#include "buffer.h"
#include <queue>
#include <vector>

static constexpr size_t MEM_BUF = 1 << 20; // 8MB 内存缓冲

static void two_way_merge(RunStore &store,
                          uint32_t id1, uint32_t id2,
                          RunStore &out_store) 
{
    InputBuffer buf1(store, id1);
    InputBuffer buf2(store, id2);
    OutputBuffer out_buf(out_store);

    /* 归并两个已排序序列 */
    while (buf1.has_next() && buf2.has_next()) {
        if (buf1.peek() <= buf2.peek()) {
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
}

std::unique_ptr<MergePlanNode>
make_binary_merge_plan(RunStore &store,
                       std::vector<uint32_t> runs) {
    if(runs.empty()) return nullptr;

    /* 首先为每个 run 构造叶子节点 */
    std::vector<std::unique_ptr<MergePlanNode>> trees;
    for(auto id : runs) {
        auto[p, n] = store.get_run(id);
        trees.push_back(std::make_unique<MergePlanNode>(true,  store.path(), n));
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
        pq.push(std::move(parent));
    }
    return pq.empty() ? nullptr : std::move(const_cast<std::unique_ptr<MergePlanNode>&>(pq.top()));
}

/* 执行操作计划 */
void excute_merge_plan(MergePlanNode *root,
                       RunStore &in_store,
                       RunStore &out_store)
{
    if(root == nullptr) return;
    if(root->is_leaf) return;
    excute_merge_plan(root->left.get() , in_store, out_store);
    excute_merge_plan(root->right.get(), in_store, out_store);

    uint32_t id1 = root->left->id;
    uint32_t id2 = root->right->id;
    two_way_merge(in_store, id1, id2, out_store);
    root->id = out_store.run_count() - 1; // 记录新 run 编号
}