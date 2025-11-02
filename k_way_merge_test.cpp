// TODO : 实现k路归并测试代码
#include <iostream>
#include "k_way_merge_threads.h"
#include <vector>

int main() {
    int K = 4;
    int total_runs = 9;
    std::vector<std::vector<int>> tasks;
    tasks = generate_task(K, total_runs);
    for(std::vector t: tasks) {
        for(int i = 0; i < t.size(); i++) {
            std::cout << t[i] << " ";
        }
        std::cout<<std::endl;
    }
    return 0;
}
