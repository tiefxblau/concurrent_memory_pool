#pragma once
#include "common.h"
#include "threadCache.h"
#include "centralCache.h"
#include "pageCache.h"

// 定长内存池，用于代替new ThreadCache
static ObjectPool<ThreadCache> objPool;

void* ConcurrentAlloc(std::size_t bytes)
{
	if (pTLS_threadCache == nullptr)
	{
		std::unique_lock<std::mutex> lk(objPoolMtx);
		pTLS_threadCache = objPool.New();
		lk.unlock();
	}

	// 若大于kMaxBytes，直接向pageCache获取内存
	if (bytes > kMaxBytes)
	{
		std::size_t realBytes = SizeClass::RoundUp(bytes);
		PageCache& pageCache = PageCache::GetInstance();

		// 记录大小
		Span* span = pageCache.FetchSpan(bytes >> kPageShift);
		span->obj_size = realBytes;
		return (void*)((std::size_t)span->page_id << kPageShift);
	}
	else
	{
		return pTLS_threadCache->Allocate(bytes);
	}
}

void ConcurrentDealloc(void* ptr)
{
	assert(pTLS_threadCache);
	
	std::size_t id = (std::size_t)ptr >> kPageShift;
	PageCache& pageCache = PageCache::GetInstance();
	Span* span = PageCache::_idSpanMap.get(id);

	// 若大于kMaxBytes，直接向pageCache释放内存
	if (span->obj_size > kMaxBytes)
	{
		pageCache.ReleaseSpanToPageCache(span);
	}
	else
	{
		pTLS_threadCache->Deallocate(ptr);
	}
}