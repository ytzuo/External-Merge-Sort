# CMakeList.txt: data_structure_specialized_pratice 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("data_structure_specialized_pratice")

# 包含头文件目录
include_directories(include)
# 添加所有源文件
add_executable(data_structure_specialized_pratice
    src/data_structure_specialized_pratice.cpp
    src/FileHelper.cpp
    src/InputBuffer.cpp
    src/OutputBuffer.cpp
    src/MergeSortFile.cpp
    src/TwoWayMerger.cpp
    src/testGenRun.cpp
)
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET data_structure_specialized_pratice PROPERTY CXX_STANDARD 20)
endif()

# TODO: 如有需要，请添加测试并安装目标。
