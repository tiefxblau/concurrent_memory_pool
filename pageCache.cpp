#include "./pageCache.h"

PageCache::PageMap PageCache::_idSpanMap;
PageCache PageCache::_ins;

Span* PageCache::FetchSpan(std::size_t pageNum)
{
	assert(pageNum > 0);

	std::unique_lock<std::recursive_mutex> lk(_pageMtx);

	// 若pageNum大于pagecache的最大页数限制，直接向系统申请
	if (pageNum >= kNPageList)
	{
		void* ptr = SystemAlloc(pageNum << kPageShift);

		// 与下文保持一致，建立一个span
		Span* span = spanPool.New();
		span->freeList.ReplaceHead(ptr, 1);
		span->page_id = (std::size_t)ptr >> kPageShift;
		span->page_num = pageNum;

		_idSpanMap.set(span->page_id, span);

		return span;
	}

	// 如果pageNum页spanlist非空，直接返回
	if (!_spanLists[pageNum].empty())
	{
		// 建立res中所有页号与res的映射
		Span* res = _spanLists[pageNum].pop_front();
		res->is_used = true;
		for (std::size_t id = res->page_id; id < res->page_num + res->page_id; ++id)
		{
			_idSpanMap.set(id, res);
		}
		return res;
	}

	// 走到这里说明pageNum页spanlist为空，向后寻找更大的span
	for (int i = pageNum + 1; i < kNPageList; ++i)
	{
		if (!_spanLists[i].empty())
		{
			Span* theSpan = _spanLists[i].pop_front();
			Span* res = spanPool.New();

			// 尾切
			res->is_used = true;
			res->page_id = theSpan->page_id + theSpan->page_num - pageNum;
			res->page_num = pageNum;
			// 建立res中所有页号与res的映射
			for (std::size_t id = res->page_id; id < res->page_num + res->page_id; ++id)
			{
				_idSpanMap.set(id, res);
			}

			theSpan->page_num -= pageNum;
			_spanLists[theSpan->page_num].push_front(theSpan);

			// 因为page cache中的span是未使用的，所以只需建立首尾页号的映射
			_idSpanMap.set(theSpan->page_id, theSpan);
			_idSpanMap.set(theSpan->page_id + theSpan->page_num - 1, theSpan);

			return res;
		}
	}

	// 找不到更大的span，就向系统申请
	void* ptr = SystemAlloc((kNPageList - 1) << kPageShift);
	Span* newSpan = spanPool.New();
	newSpan->page_id = (std::size_t)ptr >> kPageShift;
	newSpan->page_num = kNPageList - 1;
	newSpan->freeList.push_front(ptr);

	_idSpanMap.set(newSpan->page_id, newSpan);
	_idSpanMap.set(newSpan->page_id + newSpan->page_num - 1, newSpan);

	_spanLists[kNPageList - 1].push_front(newSpan);

	return FetchSpan(pageNum);

}

void PageCache::ReleaseSpanToPageCache(Span* span)
{
	std::unique_lock<std::recursive_mutex> lk(_pageMtx);

	// 大页span直接向系统释放
	if (span->page_num >= kNPageList)
	{
		//_idSpanMap.erase(span->page_id);
		SystemDealloc(span->freeList.begin(), span->obj_size);
		spanPool.Delete(span);
		return;
	}

	span->is_used = false;
	while (true)
	{
		// 寻找前一个span
		Span* ret = _idSpanMap.get(span->page_id - 1);
		if (ret == nullptr)
			break;

		Span* prevSpan = ret;
		// 下一个span正被central cache使用，停止合并
		if (prevSpan->is_used)
			break;
		// 合并后页数大于最大页数，停止合并
		if (prevSpan->page_num + span->page_num > kNPageList - 1)
			break;

		// 合并当前span和前一个span
		_spanLists[prevSpan->page_num].erase(prevSpan);

		span->page_id = prevSpan->page_id;
		span->page_num += prevSpan->page_num;
		_idSpanMap.set(prevSpan->page_id, span);

		spanPool.Delete(prevSpan);
	}

	while (true)
	{
		// 寻找后一个span
		auto ret = _idSpanMap.get(span->page_id + span->page_num);
		if (ret == nullptr)
			break;

		Span* nextSpan = ret;
		// 下一个span正被central cache使用，停止合并
		if (nextSpan->is_used)
			break;
		// 合并后页数大于最大页数，停止合并
		if (nextSpan->page_num + span->page_num > kNPageList - 1)
			break;

		// 合并当前span和后一个span
		_spanLists[nextSpan->page_num].erase(nextSpan);

		span->page_num += nextSpan->page_num;
		_idSpanMap.set(nextSpan->page_id + nextSpan->page_num - 1, span);

		spanPool.Delete(nextSpan);
	}
	//  将合并完成的span放入spanlists中
	_spanLists[span->page_num].push_front(span);
}


