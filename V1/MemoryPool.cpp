#include "MypoolV1.h"

namespace memoryPoolV1 {
	MemoryPool::MemoryPool(size_t BlockSize)
		: BlockSize_(BlockSize)
		, SlotSize_(0)
		, firstSlot_(nullptr)
		, curSlot_(nullptr)
		, freeList_(nullptr)
		, lastSlot_(nullptr){}

	MemoryPool::~MemoryPool() { // 内存池析构：需要将所有的块回收,遍历firstSlot_
		// 把连续的block删除
		Slot* cur = firstSlot_;
		while (cur)
		{
			Slot* next = cur->next;
			// 等同于 free(reinterpret_cast<void*>(firstBlock_));
			// 转化为 void 指针，因为 void 类型不需要调用析构函数，只释放空间
			operator delete(reinterpret_cast<void*>(cur));
			cur = next;
		}
	}

	void MemoryPool::init(size_t size)
	{
		assert(size > 0);
		SlotSize_ = size;
		firstSlot_ = nullptr;
		curSlot_ = nullptr;
		freeList_ = nullptr;
		lastSlot_ = nullptr;
	}
	void* MemoryPool::allocate() { // 先判断空闲链表中有没有块，没有再去找当前分配的块，不够申请内存扩容allocateNewBlock
		Slot* slot = popFreeList();
		if (slot != nullptr) return slot;
		// 接下来就是不够的情况
		{
			std::lock_guard<std::mutex> lock(mutexForBlock_);
			if (curSlot_ >= lastSlot_) {
				allocateNewBlock();
			}
			slot = curSlot_;
			curSlot_ += SlotSize_ / sizeof(Slot);  // curSlot_是slot类型指针，因此需要按Slot的倍数加
		}
		return slot;
	}

	void MemoryPool::deallocate(void* ptr) { // 传入的void*是一个纯内存块
		if (ptr == nullptr || sizeof(ptr) != SLOT_BASE_SIZE)return;
		pushFreeList(reinterpret_cast<Slot*>(ptr));
	}

	void MemoryPool::allocateNewBlock() {

		//std::cout << "申请一块内存块，SlotSize: " << SlotSize_ << std::endl;
		// 头插法插入新的内存块
		void* newBlock = operator new(BlockSize_);
		reinterpret_cast<Slot*>(newBlock)->next = firstSlot_;  // 那么现在分配的内存块如何通过析构销毁资源呢？
		firstSlot_ = reinterpret_cast<Slot*>(newBlock);

		char* body = reinterpret_cast<char*>(newBlock) + sizeof(Slot*);
		size_t paddingSize = padPointer(body, SlotSize_); // 计算对齐需要填充内存的大小
		curSlot_ = reinterpret_cast<Slot*>(body + paddingSize);

		// 超过该标记位置，则说明该内存块已无内存槽可用，需向系统申请新的内存块
		lastSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<size_t>(newBlock) + BlockSize_ - SlotSize_ + 1);

		freeList_ = nullptr;
	}

	size_t MemoryPool::padPointer(char* p, size_t align) {
		// align 是槽大小
		return (align - reinterpret_cast<size_t>(p)) % align;
	}

	// 使用CAS操作进行无锁入队和出队
	bool MemoryPool::pushFreeList(Slot* slot) {
		while (true) {
			Slot* oldHead = freeList_.load(std::memory_order_relaxed); // 为什么要这样设置内存模型？
			slot->next.store(oldHead, std::memory_order_relaxed);

			// 尝试将新节点设置为头节点
			if (freeList_.compare_exchange_weak(oldHead, slot,
				std::memory_order_release, std::memory_order_relaxed)) // 成功：写操作，失败：无序
			{
				return true;
			}
        // 失败：说明另一个线程可能已经修改了 freeList_
        // CAS 失败则重试
		}
	}
	Slot* MemoryPool::popFreeList() {
		
		while (true) {
			Slot* oldHead = freeList_.load(std::memory_order_relaxed);
			if (oldHead == nullptr)return nullptr;
			Slot* newHead = oldHead->next.load(std::memory_order_relaxed); // 为什么要这样设置内存模型？
			if (freeList_.compare_exchange_weak(oldHead, newHead,
				std::memory_order_acquire, std::memory_order_relaxed)) { // 成功：读操作，失败：无序
				return oldHead;
			}
        // 失败：说明另一个线程可能已经修改了 freeList_
        // CAS 失败则重试
    }
}


void HashBucket::initMemoryPool()
{
    for (int i = 0; i < MEMORY_POOL_NUM; i++)
    {
        getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
    }
		}

// 单例模式
MemoryPool& HashBucket::getMemoryPool(int index)
{
    static MemoryPool memoryPool[MEMORY_POOL_NUM];
    return memoryPool[index];
	}

}
