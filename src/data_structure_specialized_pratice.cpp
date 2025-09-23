// data_structure_specialized_pratice.cpp: 定义应用程序的入口点。
//

#include "../include/data_structure_specialized_pratice.h"
#include "../include/FileHelper.h"
#include <iostream>
#include <vector>
#include <cassert>

using namespace std;
void test1();
void test2();
int main()
{
	cout << "Hello CMake." << endl;
	test1();
    std::cout<<"__________________________________________________________"<<std::endl;
    test2();
	return 0;
}


void test1() {
    try {
        // 确保files目录存在
        //std::filesystem::create_directories("./files");
        
        // 测试1: 生成测试文件
        std::cout << "Generating test file..." << std::endl;
        FileHelper helper;
        helper.genRawFile("test.dat", 100, (uint64_t)5, double(0)); // 生成5个run，每个包含100个int
        
        // 测试2: 读取头部信息
        std::cout << "Reading header..." << std::endl;
        FileHelper::Header header;
        bool result = helper.readHeader("test.dat", &header);
        //std::cout<<"run_count: "<<header.run_count<<std::endl;
        if (!result) {
            std::cerr << "Failed to read header!" << std::endl;
            return;
        }
        
        assert(header.run_count == 5);
        //std::cout << "Run count: " << header.run_count << std::endl;
        
        // 测试3: 使用第一个scan函数读取数据
        std::cout << "Testing scan function 1..." << std::endl;
        std::vector<double> buffer(50); // 分配足够内存
        auto* begin_ptr = buffer.data();
        auto* end_ptr = buffer.data();
        
        // 调用scan函数，读取第1个run（pos=1）中的最多50个元素
        auto positions = helper.scan("test.dat", 50, 1, begin_ptr, end_ptr);
        std::cout << "Read ended at position: " << positions.first << std::endl;
        std::cout << "Run ends at position: " << positions.second << std::endl;
        std::cout << "Elements read: " << (end_ptr - begin_ptr) << std::endl;
        
        // 显示前几个元素
        std::cout << "First 10 elements: ";
        for (int i = 0; i < std::min(10, static_cast<int>(end_ptr - begin_ptr)); ++i) {
            std::cout << begin_ptr[i] << " ";
        }
        std::cout << std::endl;
        
        // 测试4: 使用第二个scan函数读取数据
        std::cout << "Testing scan function 2..." << std::endl;
        std::vector<double> buffer2(30);
        auto* begin_ptr2 = buffer2.data();
        auto* end_ptr2 = buffer2.data();
        
        // 从第一次读取结束的位置开始，继续读取最多20个元素
        std::streampos end_pos = positions.second;
        std::streampos start_pos = positions.first;
        std::cout << "Second scan: start_pos=" << start_pos << ", end_pos=" << end_pos << std::endl;
        auto end_read_pos = helper.scan("test.dat", start_pos, end_pos, 20, begin_ptr2, end_ptr2);
        
        std::cout << "Read ended at position: " << end_read_pos << std::endl;
        std::cout << "Elements read: " << (end_ptr2 - begin_ptr2) << std::endl;
        
        // 显示读取的元素
        std::cout << "Read elements: ";
        for (int i = 0; i < (end_ptr2 - begin_ptr2); ++i) {
            std::cout << begin_ptr2[i] << " ";
        }
        std::cout << std::endl;
        
        std::cout << "All tests passed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void test2() {
    try {
        std::string file_name = "test2.dat";
        int run_count = 10;
        int run_length = 100;
        FileHelper helper;
        std::streampos put_pos = helper.genEmptyFile(file_name, run_count, int(0));
        std::cout<<"空文件创建完成!"<<std::endl;

        std::vector<std::vector<int>> new_data;
        for(int i = 1; i <= run_count; i++) {
            std::vector<int> temp;
            for(int j = 1; j <= run_length; j++) {
               temp.push_back(i+j);
            }
            new_data.push_back(temp);
        }
        std::cout<<"待插入数据创建完成!"<<std::endl;
        for(int i = 1; i <= run_count; i++) {
            int* data = new_data[i-1].data();
            put_pos = helper.put(file_name, put_pos, run_length, i, data, data+run_length);
        }
        std::cout<<"数据已插入文件!"<<std::endl;

        std::vector<int> data1(10);
        int* begin = data1.data();
        int* end = data1.data();
        auto position = helper.scan(file_name, 10, 1, begin, end);
        cout<<"First 10 elements: ";
        //std::cout<<static_cast<int>(end-begin)<<std::endl;
        for(int i = 0; i < static_cast<int>(end-begin); i++) {
            cout<<data1[i]<<" ";
            //begin++;
        } cout<<std::endl;

        std::vector<int> data2(10);
        begin = data2.data();
        end = data2.data();
        auto current = helper.scan(file_name, position.first, position.second, 10, begin, end);
        cout<<"Then 10 elements: ";
        for(int i = 0; i < static_cast<int>(end-begin); i++) {
            cout<<data2[i]<<" ";
            //begin++;
        } cout<<std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}