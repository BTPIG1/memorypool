#include "ThreadCache.h"
#include"CentralCache.h"

namespace MymemoryPoolV2 {
	void* ThreadCache::allocate(size_t size) {
		// 处理0大小的分配请求
		if (size == 0)
		{
			size = ALIGNMENT; // 至少分配一个对齐大小
		}

		if (size > MAX_BYTES)
		{
			// 大对象直接从系统分配
			return malloc(size);
		}

		size_t index = SizeClass::getIndex(size);
		
		// 我自己写的，应该功能与原来一致
		freeListSize_[index]--;
		void* result = freeList_[index];
		if (result != nullptr) {
			freeList_[index] = *reinterpret_cast<void**>(result);
			*reinterpret_cast<void**>(result) = nullptr;
			return result;
		}
		return fetchFromCentralCache(index);
	}

	void ThreadCache::deallocate(void* ptr, size_t size) { // 如果使用operator new出来的好像不用size也可以
		if (ptr == nullptr)return;
		if (size > MAX_BYTES)
		{
			free(ptr);
			return;
		}
		size_t index = SizeClass::getIndex(size);

		// 插入到线程本地自由链表
		*reinterpret_cast<void**>(ptr) = freeList_[index];
		freeList_[index] = ptr;

		// 更新自由链表大小
		freeListSize_[index]++; // 增加对应大小类的自由链表大小

		// 判断是否需要将部分内存回收给中心缓存
		if (shouldReturnToCentralCache(index))
		{
			returnToCentralCache(freeList_[index], size);
		}
	}

	// 从中心缓存获取内存
	void* ThreadCache::fetchFromCentralCache(size_t index) { // 那么必定是index下标内存池不够用
		// 从中心缓存批量获取内存
		void* start = CentralCache::getInstance().fetchRange(index); // 这里返回的是什么结构的?!!
		if (!start)return nullptr;

		// 这里result需要被分配，后续内存需要插入freeList_
		void* result = start;
		freeList_[index] = *reinterpret_cast<void**>(start); // 获取到result开始第一个8字节得到一个地址-》下一个块地址

		size_t batchNum = 0;
		void* current = start;

		// 计算从中心缓存获取的内存块数量
		while (current != nullptr)
		{
			batchNum++;
			current = *reinterpret_cast<void**>(current); // 遍历下一个内存块
		}

		// 更新freeListSize_，增加获取的内存块数量
		freeListSize_[index] += batchNum;

		return result;
	}

	// 归还内存到中心缓存
	void ThreadCache::returnToCentralCache(void* start, size_t size) {
		// 1. 计算内存池下标和对齐后的大小、内存块数量
		size_t index = SizeClass::getIndex(size);
		size_t alignedSize = SizeClass::roundUp(size);
		size_t totalBatchNum = freeListSize_[index];
		if (totalBatchNum <= 1) return;

		// 2. 计算需要返回的内存块数量
		size_t keepNum = std::max(totalBatchNum / 4, size_t(1));
		size_t returnNum = totalBatchNum - keepNum;

		// 3. 将内存块串成链表
		char* current = reinterpret_cast<char*>(start);
		char* splitNode = current;
		for (size_t i = 0; i < keepNum-1; i++) {
			splitNode = reinterpret_cast<char*>(*reinterpret_cast<void**>(splitNode));
			if (splitNode == nullptr) {
				returnNum = totalBatchNum - i + 1;
				break;
			}
		}

		if (splitNode != nullptr) {
			void* nextNode = *reinterpret_cast<void**>(splitNode);
			*reinterpret_cast<void**>(splitNode) = nullptr;

			// 更新ThreadCache的空闲链表
			freeList_[index] = start;

			// 更新自由链表大小
			freeListSize_[index] = keepNum;

			// 将剩余部分返回给CentralCache
			if (returnNum > 0 && nextNode != nullptr)
			{
				CentralCache::getInstance().returnRange(nextNode, returnNum * alignedSize, index);
			}
		}
	}

	// 判断内存块是否需要回收给中心缓存
	bool ThreadCache::shouldReturnToCentralCache(size_t index) {
		// 设定阈值，例如：当自由链表的大小超过一定数量时
		size_t threshold = 64; // 例如，64个内存块
		return (freeListSize_[index] > threshold);
	}
}