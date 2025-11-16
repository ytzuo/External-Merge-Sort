目录中的文件涉及全部的三个项目

project1：基于二路归并计划树的外部排序实现

文件：

    multi_run_file.h, multi_run_file.cpp => 支持多归并段存储的文件，采用将元数据放在文件末尾的形式，保证可扩展性
    
    buffer.h, buffer.cpp                 => 缓冲区类
    
    run_store.h, run_store.cpp           => 封装文件操作，提供对段的多种必要操作方法
    
    merge_plan.h, merge_plan.cpp         => 归并计划树的实现，包括生成归并计划树的逻辑和
    
    merge_plan_tree_test.cpp             => 归并计划树的测试代码

project2：基于三线程并行模型的初始归并段生成

文件：

    threads.h, threads.cpp                => 三线程并行模型的实现，采用缓冲区 
    
    multi_thread_test.cpp                 => 三线程并行模型的测试代码

project3：基于三线程模型的多路归并的归并算法

文件：

    k_way_merge_theads.h, k_way_merge_theads.cpp => 基于三线程模型的多路归并的归并算法
    
    k_way_merge_threads_test.cpp                 => 基于三线程模型的多路归并的归并算法测试代码

使用cmake构建，建议执行以下命令进行构建：
```bash
    cd path/to/project
    mkdir build
    cd build
    cmake ..
    make
```
