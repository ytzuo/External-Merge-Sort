// FileHelper.h
#pragma once

#include <string>
#include <fstream>
#include <utility>
#include <filesystem>
#include <vector>
#include <random>
#include <algorithm>
#include <iostream>

/* ---------- 工具：类型 → 1 B ID ---------- */
template<typename T>
struct TypeMap {
    static constexpr uint8_t id = 0xFF; //未特化的初始id, 默认报错
};
template<> struct TypeMap<int> { static constexpr uint8_t id = 0x01; };
template<> struct TypeMap<float> { static constexpr uint8_t id = 0x10; };
template<> struct TypeMap<double> { static constexpr uint8_t id = 0x11; };


class FileHelper {
public:

    /* ---------- 24 B 元数据项 ---------- */
    struct Meta {
        uint64_t run_len;      // 元素个数
        uint64_t run_offset;   // 文件偏移
        uint32_t crc32;        // 本 run 原始字节 CRC 用于校验数字完整性
        uint32_t reserved = 0; // 保留字段
    } __attribute__((packed)); // 强制编译器不进行内存对齐优化

    /* ---------- 32 B 文件头部 ---------- */
    struct Header {
        uint32_t magic = 0x004D534F; // "OSM\0"
        uint8_t  version = 1;
        uint8_t  type;                 // 元素类型
        uint8_t  user_size = 0;        // 仅当 type==0x20 时有效, 面对未来可能的用户自定义类型扩展
        uint8_t  reserved1 = 0;        // 保留
        uint64_t run_count;            // 小段存数
        uint8_t  reserved2[13] = { 0 }; // 保留
    } __attribute__((packed));

    /* ---------- CRC32 简易实现（可替换为 zlib::crc32） ---------- */
    static uint32_t crc32(const void* data, std::size_t n) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        uint32_t crc = 0xFFFFFFFF;
        for (std::size_t i = 0; i < n; ++i) {
            crc ^= p[i];
            for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
        return ~crc;
    }
    
    //如果输入是 0x1234567890ABCDEF，输出将是 0xEFCDAB9078563412。
    inline uint64_t byteswap64(uint64_t x) {
        return  ((x & 0xFF00000000000000ull) >> 56) |
            ((x & 0x00FF000000000000ull) >> 40) |
            ((x & 0x0000FF0000000000ull) >> 24) |
            ((x & 0x000000FF00000000ull) >> 8) |
            ((x & 0x00000000FF000000ull) << 8) |
            ((x & 0x0000000000FF0000ull) << 24) |
            ((x & 0x000000000000FF00ull) << 40) |
            ((x & 0x00000000000000FFull) << 56);
    }
    
    inline uint32_t byteswap32(uint32_t x) {
        return  ((x & 0xFF000000) >> 24) |
            ((x & 0x00FF0000) >> 8) |
            ((x & 0x0000FF00) << 8) |
            ((x & 0x000000FF) << 24);
    }
    
    inline uint64_t le64toh_custom(uint64_t x) { return byteswap64(x); }

    template<typename T>
    static std::vector<T> makeSortedRun(int len) {
        static std::mt19937_64 rng(42);
        std::vector<T> ret(len);

        if constexpr (std::is_floating_point_v<T>) { // 类型推断
            std::uniform_real_distribution<T> dist(0, 10000); // 确定随机数范围
            std::generate(ret.begin(), ret.end(), [&] {return dist(rng); });
        }
        else {
            std::uniform_int_distribution<T> dist(0, 10000);
            std::generate(ret.begin(), ret.end(), [&] {return dist(rng); });
        }
        std::sort(ret.begin(), ret.end()); //排序保证非减
        return ret;
    }

    // 普通成员函数声明和实现
    bool readHeader(const std::string& filename, Header* header) {
        std::string full_path = "./files/" + filename;
        std::ifstream file(full_path, std::ios::binary);
        if (!file) {
            return false;
        }

        file.read(reinterpret_cast<char*>(header), sizeof(Header));
        if (!file.good()) {
            return false;
        }
        
        // 将小端序的run_count转换为主机字节序
        header->run_count = byteswap64(header->run_count);
        header->magic = byteswap32(header->magic);
        
        return true;
    }
    // 模板函数必须在头文件中实现
    template<typename T>
    void genRawFile(std::string file_name, int len, uint64_t num, T type) {
        //using data_type = T;
        std::string filePath = "./files/" + file_name;
        std::ofstream fs(filePath);
        if (!fs) throw std::runtime_error("open file failed");

        /* 1. 写头部（先留位） */
        Header h{};
        h.type = TypeMap<T>::id;
        std::cout<<"type: "<<h.type<<std::endl;
        h.user_size = h.type == 0x20 ? (sizeof(T)) : 0;
        h.run_count = byteswap64(num);          // 主机 → 小端
        h.magic = byteswap32(h.magic);
        fs.write(reinterpret_cast<const char*>(&h), sizeof(h));

        /* 2. 预留元数据表空间 */
        std::streampos meta_pos = fs.tellp();
        fs.seekp(num * sizeof(Meta), std::ios::cur); // 保留num个Meta的空间

        /* 3. 逐 run 生成 + 写数据 + 4 KiB 对齐 */
        std::vector<Meta> metas;
        metas.reserve(num);
        //std::mt19937_64 rng{std::random_device{}()};

        for (uint64_t i = 0; i < num; i++) {
            auto run = makeSortedRun<T>(len);
            Meta m;
            m.run_len = len;
            m.run_offset = static_cast<uint64_t>(fs.tellp());

            fs.write(reinterpret_cast<const char*>(run.data()),
                run.size() * sizeof(T));

            /* 4 KiB 对齐 */
            auto end = fs.tellp();
            //auto aligned = (end + 4095) & ~4095;
            // 改成
            std::streamoff endOff = static_cast<std::streamoff>(end);          // 1. 先转整数
            std::streamoff alignedOff = (endOff + 4095) & ~4095;               // 2. 做对齐
            fs.seekp(alignedOff);                                              // 3. 再定位
            //fs.seekp(aligned);

            m.crc32 = crc32(run.data(), run.size() * sizeof(T));
            metas.push_back(m);
        }
        /* 4. 回写元数据表 */
        fs.seekp(meta_pos);
        for (auto& m : metas) {
            m.run_len = byteswap64(m.run_len);
            m.run_offset = byteswap64(m.run_offset);
            m.crc32 = byteswap32(m.crc32);
            fs.write(reinterpret_cast<const char*>(&m), sizeof(m));
        }
        fs.close();
    }

    // 第一个scan函数模板实现, 用于首次读取文件中的一条run, 返回读取完成时的文件指针和本条run末尾的指针
    template<typename T>
    std::pair<std::streampos, std::streampos> scan(
        std::string filename,
        int size,
        int pos,
        T*& begin,
        T*& end) {

        std::string filePath = "./files/" + filename;
        std::ifstream fs(filePath, std::ios::binary);
        if (!fs) throw std::runtime_error("Cannot open file: " + filePath);

        Header header;
        readHeader(filename,&header);
        uint8_t type = header.type;
        std::cout<<"type: "<<type<<std::endl;

        // 验证type与模板参数T是否匹配
        uint8_t expected_type = 0xFF; // 默认无效值
        if (std::is_same_v<T, int>) {
            expected_type = 0x01;
        } else if (std::is_same_v<T, float>) {
            expected_type = 0x10;
        } else if (std::is_same_v<T, double>) {
            expected_type = 0x11;
        }

        if (type != expected_type) {
            fs.close();
            throw std::runtime_error("Type mismatch: expected type " + std::to_string(expected_type) + 
                                   ", but got type " + std::to_string(header.type));
        }

        // 检查pos是否有效, 超过run的最大数量则报错
        if (pos < 1 || pos > static_cast<int>(header.run_count)) {
            fs.close();
            throw std::runtime_error("Invalid position");
        }

        Meta meta;
        // 计算Meta的开始位置
        std::streampos meta_start = sizeof(Header) + static_cast<std::streamoff>(pos - 1) * sizeof(Meta);
        fs.seekg(meta_start, std::ios::beg);
        fs.read(reinterpret_cast<char*>(&meta), sizeof(Meta));
        meta.run_len = byteswap64(meta.run_len);
        meta.run_offset = byteswap64(meta.run_offset);

        // 找到run起始位置
        std::streampos run_start = meta.run_offset;
        fs.seekg(run_start);

        // 计算run结束位置
        std::streampos run_end = run_start + static_cast<std::streamoff>(meta.run_len * sizeof(T));

        // 读取数据
        int count = 0;
        T* current = begin;
        while (count < size && fs.read(reinterpret_cast<char*>(current), sizeof(T))) {
            current++;
            count++;
        }

        end = current;
        std::streampos read_end = fs.tellg();

        fs.close();

        // 返回pair：第一个元素是读取结束位置，第二个元素是run的末尾位置
        return std::make_pair(read_end, run_end);
    }

    // 第二个scan函数模板实现, 从上次读取到的位置继续向下读取
    template<typename T>
    std::streampos scan(
        std::string filename,
        std::streampos streampos,
        std::streampos endpos,
        int size,
        T*& begin,
        T*& end) {

        std::string filePath = "./files/" + filename;
        std::ifstream fs(filePath, std::ios::binary);
        if (!fs) throw std::runtime_error("Cannot open file: " + filePath);
        
        // 计算可读取的最大字节数
        std::streampos max_bytes = endpos - streampos;

        fs.seekg(streampos, std::ios::beg);
        // 确保不会读取超过一个元素大小的数据，计算实际可读取的元素数量
        size_t read_size = std::min(static_cast<size_t>(max_bytes) / sizeof(T), static_cast<size_t>(size));

        // 读取数据到begin指向的内存区域
        if (read_size > 0) {
            fs.read(reinterpret_cast<char*>(begin), read_size * sizeof(T));
            // 更新实际读取的元素数量（考虑可能读取不足的情况）
            read_size = static_cast<size_t>(fs.gcount()) / sizeof(T);
        }

        // 设置end指针
        end = begin + read_size;

        // 获取读取结束位置
        std::streampos current_pos = fs.tellg();

        fs.close();

        // 返回读取结束位置
        return current_pos;
    }

    template<typename T>
    std::streampos genEmptyFile(
        std::string file_name, 
        int run_count,          // 文件中将插入的run的个数
        T type) {

        // 创建空文件
        std::string filePath = "./files/" + file_name;
        std::ofstream fs(filePath);
        if (!fs) throw std::runtime_error("open file failed");

        // 写入文件头
        Header h;
        h.type = TypeMap<T>::id;
        std::cout<<"type: "<<h.type<<std::endl;
        h.user_size = h.type == 0x20 ? (sizeof(T)) : 0;
        h.run_count = byteswap64(run_count);          // 主机 → 小端
        h.magic = byteswap32(h.magic);
        fs.write(reinterpret_cast<const char*>(&h), sizeof(h));

        // 预留run_count个Meta, 同时找出第一个run的开头位置
        //Meta meta;
        fs.seekp(run_count*sizeof(Meta), std::ios::cur);
        
        std::streampos run_start = fs.tellp();
        fs.close();

        return run_start;
    }

    // 向文件中写入排序后的run
    template<typename T>
	std::streampos put(std::string file_name, 
        std::streampos put_pos,
        int run_len, 
        int index, 
        T* first, 
        T* last) {

        std::string filePath = "./files/" + file_name;
        std::ofstream fs(filePath, std::ios::in | std::ios::out | std::ios::binary);
        if (!fs) throw std::runtime_error("open file failed");

        // 写入first到last之间的数据
        fs.seekp(put_pos, std::ios::beg);
        size_t put_size = std::distance(first, last);
        fs.write(reinterpret_cast<const char*>(first), put_size);
        std::streampos new_put_pos = fs.tellp();
        
        // 找到对应的Meta的位置
        std::streampos meta_pos = sizeof(Header) + (index - 1) * sizeof(Meta);
        fs.seekp(meta_pos, std::ios::beg);

        // 创建元数据
        Meta meta;
        meta.run_len = std::distance(first, last);
        std::cout<<"distance: "<<meta.run_len<<std::endl;
        meta.crc32 = crc32(first, put_size);
        meta.run_offset = put_pos;
        
        // 写回新的Meta
        fs.seekp(meta_pos, std::ios::beg);
        // 字节序转换
        meta.run_len = byteswap64(meta.run_len);
        meta.run_offset = byteswap64(meta.run_offset);
        meta.crc32 = byteswap32(meta.crc32);
        fs.write(reinterpret_cast<const char*>(&meta), sizeof(Meta));

        return new_put_pos;
	}

    // 从文件中查找run
	template<typename T>
	void search(std::string file_name, 
        int size, 
        int pos, 
        T*& begin_, 
        T*& end_) {


	}
};
