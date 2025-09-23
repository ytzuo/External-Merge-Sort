#include "../include/FileHelper.h"
//#include <fstream>
#include <vector>
#include <random>
#include <algorithm>
#include <cstring>
//#include <stdexcept>
#include <cstdint>

//#include <arpa/inet.h>  // Linux/Unix 系统
//#include <winsock2.h>   // Windows 系统
	/* ---------- 工具：类型 → 1 B ID ---------- */
/*
	template<typename T>
	struct TypeMap {
		static constexpr uint8_t id = 0xFF; //未特化的初始id, 默认报错
	};
	template<> struct TypeMap<int> { static constexpr uint8_t id = 0x01; };
	template<> struct TypeMap<float> { static constexpr uint8_t id = 0x10; };
	template<> struct TypeMap<double> { static constexpr uint8_t id = 0x11; };
	inline uint64_t byteswap64(uint64_t x);
	inline uint32_t byteswap32(uint32_t x);
	template<typename T>
	static std::vector<T> makeSortedRun(int len);
	*/

	/* ---------- 24 B 元数据项 ---------- */
	/*
	struct Meta {
		uint64_t run_len;      // 元素个数
		uint64_t run_offset;   // 文件偏移
		uint32_t crc32;        // 本 run 原始字节 CRC 用于校验数字完整性
		uint32_t reserved = 0; // 保留字段
	} __attribute__((packed)); // 强制编译器不进行内存对齐优化
	*/

	/* ---------- 32 B 文件头部 ---------- */
	/*
	struct Header {
		uint32_t magic = 0x004D534F; // "OSM\0"
		uint8_t  version = 1;
		uint8_t  type;                 // 元素类型
		uint8_t  user_size = 0;        // 仅当 type==0x20 时有效, 面对未来可能的用户自定义类型扩展
		uint8_t  reserved1 = 0;        // 保留
		uint64_t run_count;            // 小段存数
		uint8_t  reserved2[13] = { 0 }; // 保留
	} __attribute__((packed));
	*/

	/* ---------- CRC32 简易实现（可替换为 zlib::crc32） ---------- */
	/*
	static uint32_t crc32(const void* data, std::size_t n) {
		const uint8_t* p = static_cast<const uint8_t*>(data);
		uint32_t crc = 0xFFFFFFFF;
		for (std::size_t i = 0; i < n; ++i) {
			crc ^= p[i];
			for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
		}
		return ~crc;
	}
	*/

	/*
	template<typename T>
	void genFile(std::string file_name, int len, uint64_t num, T type) {
		using data_type = T;
		std::string filePath = "./files/" + file_name;
		std::ofstream fs(filePath);
		if (!fs) throw std::runtime_error("open file failed");
			
		// 1. 写头部（先留位） 
		Header h{};
		h.type = TypeMap<T>::id;
		h.user_size = h.type == 0x20 ? (sizeof(T)) : 0;
		h.run_count = byteswap64(num);          // 主机 → 小端
		h.run_count = byteswap64(h.run_count);
		h.magic = byteswap32(h.magic);
		fs.write(reinterpret_cast<const char*>(&h), sizeof(h));
			
		// 2. 预留元数据表空间 
		std::streampos meta_pos = fs.tellp();
		fs.seekp(num * sizeof(Meta), std::ios::cur); // 保留num个Meta的空间

		// 3. 逐 run 生成 + 写数据 + 4 KiB 对齐 
		std::vector<Meta> metas;
		metas.reserve(num); 
		//std::mt19937_64 rng{std::random_device{}()};

		for (int i = 0; i < num; i++) {
			auto run = makeSortedRun<T>(len);
			Meta m;
			m.run_len = len;
			m.run_offset = static_cast<uint64_t>(fs.tellp());

			fs.write(reinterpret_cast<const char*>(run.data()),
				run.size() * sizeof(T));

			// 4 KiB 对齐 
			auto end = fs.tellp();
			//auto aligned = (end + 4095) & ~4095;
			// 改成
			std::streamoff endOff = static_cast<std::streamoff>(end);          // 1. 先转整数
			std::streamoff alignedOff = (endOff + 4095) & ~4095;              // 2. 做对齐
			fs.seekp(alignedOff);                                              // 3. 再定位
			//fs.seekp(aligned);

			m.crc32 = crc32(run.data(), run.size() * sizeof(T));
			metas.push_back(m);
		}
		// 4. 回写元数据表 
		fs.seekp(meta_pos);
		for (auto& m : metas) {
			m.run_len = byteswap64(m.run_len);
			m.run_offset = byteswap64(m.run_offset);
			m.crc32 = byteswap32(m.crc32);
			fs.write(reinterpret_cast<const char*>(&m), sizeof(m));
		}
		fs.close();
	}
	*/

	/*
	template<typename T>
	void put(std::string file_name, int size, int pos, T* first, T* last) {

	}

	template<typename T>
	void search(std::string file_name, int size, int pos, T*& begin_, T*& end_) {

	}
	*/

	template<typename T>
	static std::vector<T> makeSortedRun(int len) {
		static std::mt19937_64 rng(42);
		std::vector<T> ret(len);

		if constexpr (std::is_floating_point_v<T>) { // 类型推断
			std::uniform_real_distribution<T> dist(0, 10000); // 确定随机数范围
			std::generate(ret.begin(), ret.end(), [&] {dist(rng); });
		} else {
			std::uniform_int_distribution<T> dist(0, 10000);
			std::generate(ret.begin(), ret.end(), [&] {dist(rng); });
		}
		std::sort(ret.begin(), ret.end()); //排序保证非减
		return ret;
	}

	inline uint32_t byteswap32(uint32_t x) {
		return  ((x & 0xFF000000) >> 24) |
			((x & 0x00FF0000) >> 8) |
			((x & 0x0000FF00) << 8) |
			((x & 0x000000FF) << 24);
	}
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

	inline uint64_t le64toh_custom(uint64_t x) { return byteswap64(x); }

	// bool FileHelper::readHeader(const std::string& filePath, Header* h) {

	// 	std::ifstream fin(filePath, std::ios::binary);
	// 	if (!fin) return false;
	// 	fin.read(reinterpret_cast<char*>(h), sizeof(Header));
	// 	if (fin.gcount() != sizeof(Header)) return false;
	// 	h->run_count = le64toh_custom(h->run_count);   // 小端转主机
	// 	fin.close();
	// 	return true;
	// }

// 显式实例化scan模板函数，以便链接器能找到它们
template std::pair<std::streampos, std::streampos> FileHelper::scan<int>(
    std::string filename, int size, int pos, int*& begin, int*& end);

template std::streampos FileHelper::scan<int>(
    std::string filename, std::streampos streampos, std::streampos endpos, 
    int size, int*& begin, int*& end);