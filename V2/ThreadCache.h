#pragma once
#include "Common.h"


namespace MymemoryPoolV2 {

	class ThreadCache { // 线程缓存：线程私有内存池

	private:
		// 每个线程的自由链表数组
		std::array<void*, FREE_LIST_SIZE>  freeList_; // 不同槽大小内存池对应地址
		std::array<size_t, FREE_LIST_SIZE> freeListSize_; // 自由链表大小统计 

	public:
		static ThreadCache* getInstance() {  // 这个我懂每个线程自己创建
			static thread_local ThreadCache instance; 
			return &instance;
		}

		void* allocate(size_t size);

		void deallocate(void* ptr, size_t size); // 如果使用operator new出来的好像不用size也可以

	private:
		ThreadCache()
		{
			// 初始化自由链表和大小统计
			freeList_.fill(nullptr);
			freeListSize_.fill(0);
		}

		// 从中心缓存获取内存
		void* fetchFromCentralCache(size_t index);
		// 归还内存到中心缓存
		void returnToCentralCache(void* start, size_t size);

		bool shouldReturnToCentralCache(size_t index);
	};


}