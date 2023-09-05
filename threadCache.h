#pragma once
#include "common.h"

class ThreadCache
{
	// 最大申请数量_maxGetSize每次增长的值
	static constexpr int kIncrease = 1;
public:
	// 申请与释放内存
	void* Allocate(std::size_t bytes);

	void Deallocate(void* ptr);


	// 增加最大申请数量
	void IncreaseGetSize()
	{
		_maxGetSize += kIncrease;
	}
private:

	// 从central cache中获取内存
	void* FetchFromCentralCache(std::size_t bytes, std::size_t index);

	// 将thread cache中的自由链表归还span中
	void ListTooLong(std::size_t index);


	FreeList _freeLists[kNFreeList];

	// 最多可从central cache中申请的内存块的数量
	std::size_t _maxGetSize = 1;
};

// 使用TLS线程本地存储，将数据和执行的特定的线程一一对应
#ifdef _WIN32
static __declspec(thread) ThreadCache* pTLS_threadCache = nullptr;
#else
static __thread ThreadCache* pTLS_threadCache = nullptr;
#endif
