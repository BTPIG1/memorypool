#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>

namespace memoryPoolV1 {
#define MEMORY_POOL_NUM 64
#define SLOT_BASE_SIZE 8
#define MAX_SLOT_SIZE 512

	// 内存池设计：有一个内存区域，基础块大小、总大小、第一个block、最后一个block、block队列
	// block如何设计？下标和下一个block
	// block队列如何设计？ 一个头一个尾？ 好像一个尾也行
	struct Slot {
		std::atomic<Slot*> next; // 原子指针
	};
	class MemoryPool {
	public:
		MemoryPool(size_t BlockSize_ = 4096);  // 函数声明与定义分离的好处？ 1. 隐藏实现细节 2.解耦，类与具体函数实现之间相互独立提高灵活性 3.增强复用
		~MemoryPool();

		void init(size_t);

		void* allocate();

		void deallocate(void*);

	private:
		std::atomic<Slot*> freeList_;
		Slot* firstSlot_;
		Slot* curSlot_;
		Slot* lastSlot_;

		int BlockSize_; // 内存池总大小
		int SlotSize_; // 一个块大小
		//std::mutex mutexForFreeList_; // 保证多线程下对空闲链表操作原子性
		std::mutex mutexForBlock_;

	private:
		void allocateNewBlock(); 
		size_t padPointer(char* p, size_t align);

		// 使用CAS操作进行无锁入队和出队
		bool pushFreeList(Slot* slot);
		Slot* popFreeList();
	};

	// 定义HashBucket： 类似于哈希表，根据分配内存的大小映射到不同的内存池，注意：这是无模板的，因为并不在乎数据类型，只关注数据的内存大小
	class HashBucket {
	public: // 提供给外部的方法：初始化内存池、获取特定内存池、分配内存、释放内存
		static void initMemoryPool();
		static MemoryPool& getMemoryPool(int index);

		static void* useMemory(size_t size) {
			if (size < 0)return nullptr;
			if (size > MAX_SLOT_SIZE)return operator new(size); // operator new 是一个底层的内存分配函数，不会调用构造函数，单纯分配指定大小的内存块
			return getMemoryPool((size + SLOT_BASE_SIZE - 1)/SLOT_BASE_SIZE-1).allocate();
		}

		static void freeMemory(void* ptr,size_t size) {
			if (size < 0)return;
			if (size > MAX_SLOT_SIZE) {
				operator delete(ptr); 
				return;
			}

			getMemoryPool((size + SLOT_BASE_SIZE - 1) / SLOT_BASE_SIZE - 1).deallocate(ptr);
		}

		template<typename T,typename... Args>  // typename 与 class区别？ 大部分情况等价，只有模板嵌套时必须使用typename
		friend T* newElement(Args&&... args);

		template<typename T>
		friend void deleteElement(T*);
	};

	template<typename T, typename ...Args>  // typename... Args可变参数模板；Args... args 接受多个参数；sizeof...(Args)获取参数个数；print(args...)展开参数包；std::forward<Args>(args)...完美转发
	T* newElement(Args&&... args) {
		T* p = nullptr;
		if (p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T))) != nullptr) {  // p是指向T大小的内存，并未初始化
			new(p) T(std::forward<Args>(args)...); // ？？这是什么操作？ new(p)表示在内存块p上构造T对象；std::forward<Args>(args)...完美转发当前函数的参数给T的构造函数
		}
		return p;
	}

	template<typename T>
	void deleteElement(T* p) {

		if (p) { // ？？为什么调用析构后还要归还内存？ 
			//在普通的new\delete场景下，会先销毁内部状态与资源，然后归还内存到堆分配器。
			// 
			p->~T();
			HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));
		}
	}
}