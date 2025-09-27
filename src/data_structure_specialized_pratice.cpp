// data_structure_specialized_pratice.cpp: 定义应用程序的入口点。
//

#include "../include/data_structure_specialized_pratice.h"
#include "../include/FileHelper.h"
#include <fstream>
#include <iostream>
//#include <pthread.h>
#include <vector>
#include <cassert>

using namespace std;
void test1();
void test2();
int main()
{
	cout << "Hello CMake." << endl;
	test1();
    //std::cout<<"__________________________________________________________"<<std::endl;
    //test2();
	return 0;
}

void test1() {
    cout<<"----------------------------开始test1()----------------------------"<<endl;
    string file_name   = "test1.dat";
    string file_path   = "./files/" + file_name;
    uint64_t run_count = 100;
    int run_len        = 100;
    FileHelper helper;
    helper.genRawFile(file_name, run_len, run_count, int(0));
    cout<<"完成genRawFile()"<<endl;

    fstream fs(file_path, std::ios::binary | std::ios::in);
    auto [cur_, end_] = helper.getRunInfo<int>(fs, 1);
    cout<<"完成getRunInfo()"<<endl;
    vector<int> v1(10);
    auto first1 = v1.data();
    auto last1  = v1.data();
    cur_ = helper.scan(fs, cur_, end_, 10, first1, last1);
    cout<<"第一次读取10个元素: ";
    for(int i = 0; i < 10; i++) {
        cout<<v1[i]<<" ";
    }
    cout<<endl;

    vector<int> v2(20);
    auto first2 = v2.data();
    auto last2  = v2.data();
    cur_ = helper.scan(fs, cur_, end_, 20, first2, last2);
    cout<<"第二次读取20个元素: ";
    for(int i = 0; i < 20; i++) {
        cout<<v2[i]<<" ";
    }
    cout<<endl;

    fs.close();
    cout<<"----------------------------test1()完成----------------------------"<<endl;
}