#pragma once
#include "common.h"
#include "pageMap.h"

// 同central cache为单例模式
class PageCache
{
#ifdef _WIN32 //windows x86 or x64

#ifdef _WIN64 //x64 windows
	typedef PageMap3<64 - kPageShift> PageMap;
#else //x86 windows
	typedef PageMap2<32 - kPageShift> PageMap;
#endif //end of _WIN64

#else //unix

#ifdef __x86_64__ //x64 unix
	typedef PageMap3<64 - kPageShift> PageMap;
#elif __i386__ //x86 unix
	typedef PageMap2<32 - kPageShift> PageMap;
#endif //end of __x86_64__

#endif //end of _WIN32
public:

	// page_id到span的映射
	//static std::unordered_map<std::size_t, Span*> _idSpanMap;
	static PageMap _idSpanMap;

	std::recursive_mutex _pageMtx;

	static PageCache& GetInstance()
	{
		return _ins;
	}

	// 从_spanList中返回有pageNum页的span
	// 线程安全，内有递归调用因此使用recursive_mutex
	Span* FetchSpan(std::size_t pageNum);

	// 将span归还给page cache, 以及执行后续的合并操作
	// 线程安全
	void ReleaseSpanToPageCache(Span* span);

private:
	PageCache() {}

	PageCache(const PageCache&) = delete;

	// 桶的下标代表当前桶存有的span的页数
	// 其中span的freelist是未经处理的整块大内存（没有记录size和next）
	SpanList _spanLists[kNPageList];

	static PageCache _ins;
};