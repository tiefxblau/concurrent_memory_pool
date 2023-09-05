#pragma once
#include "common.h"

// 单例模式-饿汉
class CentralCache
{
public:

	CentralCache(const CentralCache&) = delete;

	//	返回实例
	static CentralCache& GetInstance()
	{
		return _ins;
	}

	// 给予thread cache内存
	// begin, end: 输出参数	   fetchNum: 申请的空间数量    index: 对应桶下标
	std::size_t FetchRange(void*& begin, void*& end, std::size_t fetchNum, std::size_t index, std::size_t bytes);

	// 把begin到end归还给其对应的span
	void ReleaseToSpans(void* begin, void* end, std::size_t index);

private:
	CentralCache() {};

	// 桶的大小和kNFreeList相同
	SpanList _spanLists[kNFreeList];
	static CentralCache _ins;
};