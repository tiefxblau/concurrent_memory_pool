#pragma once
#include <mutex>
#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

// 向系统申请内存
static void* SystemAlloc(std::size_t bytes)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (ptr == nullptr)
		throw std::bad_alloc();
#else
	void *ptr = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
		throw std::bad_alloc();
#endif

	return ptr;
}

// 向系统释放内存
static void SystemDealloc(void* ptr, size_t bytes)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	munmap(ptr, bytes);
#endif
}

static std::mutex objPoolMtx;

// 定长内存池
template <class T>
class ObjectPool
{
public:
	ObjectPool() = default;
	ObjectPool(std::size_t allocSize) :_allocSize(allocSize) {}

	T* New()
	{
		T* object = nullptr;
		// 若_freeList不为空，则优先从_freeList获取内存
		if (_freeList != nullptr)
		{
			object = (T*)_freeList;
			_freeList = *(void**)_freeList;
		}
		else
		{
			// 一次分配内存的大小不小于sizeof(void*)，保证能存下一个指针
			std::size_t objectSize = (std::max)(sizeof(void*), sizeof(T));

			// 若剩余内存块大小小于所需内存的大小，则重新申请内存块
			if (_remainSize < objectSize)
			{
				_memory = (char*)SystemAlloc(_allocSize);

				_remainSize = _allocSize;
			}


			// 给object分配内存
			object = (T*)_memory;
			_memory += objectSize;
			_remainSize -= objectSize;
		}

		//  定位new 显式调用构造函数
		new(object)T();

		return object;
	}

	void Delete(T* object)
	{
		// 显示调用析构函数
		object->~T();

		// 将回收内存头插至_freeList
		*(void**)object = _freeList;
		_freeList = (void*)object;
	}
	
private:
	// 内存块头指针
	char* _memory = nullptr;

	// 内存块剩余大小
	std::size_t _remainSize = 0;

	// 已回收内存的链表头指针
	// 指针所指向的前4/8字节为下一链表节点的地址
	void* _freeList = nullptr;

	// 一次申请的内存块大小
	int _allocSize = 128 * 1024;
	
};

