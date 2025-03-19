#include"PageCache.h"
//#include <sys/mman.h>
#include <windows.h>
#include <cstring>

namespace MymemoryPoolV2
{
	// 分配指定页数的span
	void* PageCache::allocateSpan(size_t numPages) {
		std::lock_guard<std::mutex> lock(mutex_);
		// 1.先查询map里有没有空闲span，2.没有再申请内存
		auto it = freeSpans_.lower_bound(numPages);
		if (it != freeSpans_.end()) {
			Span* s = it->second;
			if (s->next) {
				freeSpans_[it->first] = s->next;
				s->next = nullptr;
			}
			else {
				freeSpans_.erase(it);
			}

			if (s->numPages >= numPages) { // 这里其实会造成分割后页数小于8而永远得不到分配（内部碎片）
				Span* newSpan = new Span;
				newSpan->numPages = s->numPages - numPages;
				newSpan->next = nullptr;
				newSpan->pageAddr = reinterpret_cast<char*>(s->pageAddr)+ numPages*PAGE_SIZE;

				// 将超出部分放回空闲Span*列表头部
				auto& list = freeSpans_[newSpan->numPages];
				newSpan->next = list;
				list = newSpan;

				s->numPages = numPages;
			}
			// 记录span信息用于回收
			spanMap_[s->pageAddr] = s;
			return s->pageAddr;
		}
		// 没有合适的span，向系统申请
		void* memory = systemAlloc(numPages);
		if (!memory) return nullptr;

		// 创建新的span
		Span* span = new Span;
		span->pageAddr = memory;
		span->numPages = numPages;
		span->next = nullptr;

		// 记录span信息用于回收
		spanMap_[memory] = span;
		return memory;
	}

	// 释放span
	void PageCache::deallocateSpan(void* ptr, size_t numPages) {
		// 1. 判断ptr、numPages是否合理
		// 2. 上锁，获取Span*以及消除freeSpans_中的节点
		std::lock_guard<std::mutex> lock(mutex_);

		// 查找对应的span，没找到代表不是PageCache分配的内存，直接返回
		auto it = spanMap_.find(ptr);
		if (it == spanMap_.end()) return;

		Span* span = it->second;

		// 尝试合并相邻的span
		void* nextAddr = static_cast<char*>(ptr) + numPages * PAGE_SIZE;
		auto nextIt = spanMap_.find(nextAddr);

		if (nextIt != spanMap_.end())
		{
			Span* nextSpan = nextIt->second;

			// 1. 首先检查nextSpan是否在空闲链表中
			bool found = false;
			auto& nextList = freeSpans_[nextSpan->numPages];

			// 检查是否是头节点
			if (nextList == nextSpan)
			{
				nextList = nextSpan->next;
				found = true;
			}
			else if (nextList) // 只有在链表非空时才遍历
			{
				Span* prev = nextList;
				while (prev->next)
				{
					if (prev->next == nextSpan)
					{
						// 将nextSpan从空闲链表中移除
						prev->next = nextSpan->next;
						found = true;
						break;
					}
					prev = prev->next;
				}
			}

			// 2. 只有在找到nextSpan的情况下才进行合并
			if (found)
			{
				// 合并span
				span->numPages += nextSpan->numPages;
				spanMap_.erase(nextAddr);
				delete nextSpan;
			}
		}

		// 将合并后的span通过头插法插入空闲列表
		auto& list = freeSpans_[span->numPages];
		span->next = list;
		list = span;
	}


	// 向系统申请内存
	void* PageCache::systemAlloc(size_t numPages) {
		size_t size = numPages * PAGE_SIZE;
		// 使用mmap分配内存
		void* ptr = VirtualAlloc(
			nullptr,          // 让系统自动选择地址
			size,             // 分配大小
			MEM_COMMIT | MEM_RESERVE, // 保留并提交物理内存
			PAGE_READWRITE    // 内存权限：可读可写
		);
		if (ptr == nullptr) {
			// 可选：获取错误码
			// DWORD error = GetLastError();
			return nullptr;
		}

		// 清零内存
		memset(ptr, 0, size);
		return ptr;
	}
}