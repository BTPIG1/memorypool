#pragma once
#include<cstddef>
#include<atomic>
#include<array> // 什么意思？


namespace MymemoryPoolV2 {
	constexpr size_t ALIGNMENT = 8; // 按8个字节倍增
	constexpr size_t MAX_BYTES = 256 * 1024; // 256KB
	constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT;// 8-256

	// 内存块头部信息
	struct BlockHeader
	{
		size_t size; // 内存块大小
		bool   inUse; // 使用标志
		BlockHeader* next; // 指向下一个内存块
	};

	// 大小类管理
	class SizeClass
	{
	public:
		static size_t roundUp(size_t bytes) // 向上对齐找到第一个对齐位置，如0x2007对齐8 -》 0x200F&0111 -> 0x2008
		{
			return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
		}

		static size_t getIndex(size_t bytes) // 给定内存大小，匹配对应的内存池. 要分配20KB，理论应该分配24KB
		{
			// 确保bytes至少为ALIGNMENT
			bytes = std::max(bytes, ALIGNMENT);
			// 向上取整除对齐大小-1
			return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
		}
	};

}