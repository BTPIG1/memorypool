#include "CentralCache.h"
#include "PageCache.h"
#include <cassert>
#include <thread>

namespace MymemoryPoolV2 {
	// 每次从PageCache获取span大小（以页为单位）
	static const size_t SPAN_PAGES = 8;

	void* CentralCache::fetchRange(size_t index) { // 分配中心缓存中的内存块给线程缓存
		if (index > FREE_LIST_SIZE)return nullptr;
		// 1. 先上锁访问centralFreeList_[index]中是否还有内存块
		while (locks_[index].test_and_set(std::memory_order_acquire)) {
			std::this_thread::yield(); // 添加线程让步，避免忙等待，避免过度消耗CPU
		}
		void* result =nullptr;
        try
        {
            // 尝试从中心缓存获取内存块
            result = centralFreeList_[index].load(std::memory_order_relaxed);

            if (!result)
            {
                // 如果中心缓存为空，从页缓存获取新的内存块
                size_t size = (index + 1) * ALIGNMENT;
                result = fetchFromPageCache(size);

                if (!result)
                {
                    locks_[index].clear(std::memory_order_release);
                    return nullptr;
                }

                // 将获取的内存块切分成小块
                char* start = static_cast<char*>(result);
                size_t blockNum = (SPAN_PAGES * PageCache::PAGE_SIZE) / size;

                if (blockNum > 1)
                {  // 确保至少有两个块才构建链表
                    for (size_t i = 1; i < blockNum; ++i)
                    {
                        void* current = start + (i - 1) * size; // 因为是char*类型指针+n操作等于内存地址+n
                        void* next = start + i * size;
                        *reinterpret_cast<void**>(current) = next; // 将current开头的一个字内容替换为下一个节点地址
                    }
                    *reinterpret_cast<void**>(start + (blockNum - 1) * size) = nullptr;

                    // 保存result的下一个节点
                    void* next = *reinterpret_cast<void**>(result);
                    // 将result与链表断开
                    *reinterpret_cast<void**>(result) = nullptr;
                    // 更新中心缓存
                    centralFreeList_[index].store(
                        next,
                        std::memory_order_release
                    );
                }
            }
            else
            {
                // 保存result的下一个节点
                void* next = *reinterpret_cast<void**>(result);
                // 将result与链表断开
                *reinterpret_cast<void**>(result) = nullptr;

                // 更新中心缓存
                centralFreeList_[index].store(next, std::memory_order_release);
            }
        }
        catch (...)
        {
            locks_[index].clear(std::memory_order_release);
            throw;
        }

        // 释放锁
        locks_[index].clear(std::memory_order_release);
        return result;
	}


    // 内存回收，start起始地址，size内存大小，index对应下标
	void CentralCache::returnRange(void* start, size_t size, size_t index) {// start内存块链表的头部
        if (start == nullptr )return;
        while (locks_[index].test_and_set(std::memory_order_acquire)) { // 为什么使用acquire读有序
            std::this_thread::yield();// 具体细节？ 让出cpu时间片
        }

        try
        {
            // 将归还的内存块插入到中心缓存的链表头部
            void* current = centralFreeList_[index].load(std::memory_order_relaxed);
            *reinterpret_cast<void**>(start) = current;
            centralFreeList_[index].store(start, std::memory_order_release);
        }
        catch (...)
        {
            locks_[index].clear(std::memory_order_release);
            throw;
        }
        locks_[index].clear(std::memory_order_release);
	}

    void* CentralCache::fetchFromPageCache(size_t size) { // 只需要分配内存即可，后续分块由fetchRange实现
        // 1. 计算实际需要的页数
        size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;

        // 2. 根据大小决定分配策略
        if (size <= SPAN_PAGES * PageCache::PAGE_SIZE)
        {
            // 小于等于32KB的请求，使用固定8页
            return PageCache::getInstance().allocateSpan(SPAN_PAGES);
        }
        else
        {
            // 大于32KB的请求，按实际需求分配
            return PageCache::getInstance().allocateSpan(numPages);
        }
    }

}