cmake_minimum_required(VERSION 3.10)
project(multi_run_sort)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O3 -Wall -Wextra -march=native")
find_package(Threads REQUIRED)
add_executable(data_structure_specialized_pratice
               main.cpp
               multi_run_file.cpp
               run_store.cpp
               merge_plan.cpp
               merge_plan_tree_test.cpp
               buffer.cpp)
target_link_libraries(data_structure_specialized_pratice Threads::Threads)