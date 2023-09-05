#include "centralCache.h"
#include "pageCache.h"

CentralCache CentralCache::_ins;

std::size_t CentralCache::FetchRange(void*& begin, void*& end, std::size_t fetchNum, std::size_t index, std::size_t bytes)
{
	// 实际获取到的内存数目 = min(freelist.size(), fetchNum);
	int actualNum = 1;

	// 相同index下的span会加锁互斥
	std::unique_lock<std::mutex> lk(_spanLists[index]._mtx);

	Span* span = _spanLists[index].GetOneSpan();
	//assert(span && !span->freeList.empty());

	// 如果没有找到非空的span, 就向page cache获取一个
	if (span == nullptr)
	{
		// 这里不涉及临界资源，可先解锁
		lk.unlock();

		PageCache& pageCache = PageCache::GetInstance();
		span = pageCache.FetchSpan(SizeClass::NumOfMovePage(bytes));

		// 从page cache获取的span需要手动连好自由链表
		char* begin = (char*)(span->page_id << kPageShift);
		char* end = begin + (span->page_num << kPageShift);

		std::size_t len = 1;
		void* cur = begin;
		while ((char*)cur + bytes < end)
		{
			FreeList_next(cur) = (char*)cur + bytes;
			cur = FreeList_next(cur);
			++len;
		}
		FreeList_next(cur) = nullptr;
		span->freeList.ReplaceHead(begin, len);

		// 记录申请对象的大小
		span->obj_size = bytes;

		lk.lock();

		_spanLists[index].push_back(span);
	}

	// 将span->freeList的前fetchNum（或更少）个节点返回
	end = span->freeList.begin();
	begin = end;
	int i = 1;
	while (FreeList_next(end) != nullptr && i < fetchNum)
	{
		++actualNum;
		++i;
		end = FreeList_next(end);
	}

	// 删除span的freelist的begin到end节点
	span->freeList.pop_front(end, i);
	span->use_count += i;
	FreeList_next(end) = nullptr;

	return actualNum;
}

void CentralCache::ReleaseToSpans(void* begin, void* end, std::size_t index)
{
	std::unique_lock<std::mutex> lk(_spanLists[index]._mtx);
	// 依次遍历所有节点
	void* cur = begin;
	while (cur != nullptr)
	{
		// 先保存下一个节点，防止span->freeList.push_front(cur)后无法找到
		void* next = FreeList_next(cur);

		// 找到对应页号
		std::size_t id = (std::size_t)cur >> kPageShift;
		// 通过映射找到span
		//PageCache& pageCache = PageCache::GetInstance();
		//pageCache._pageMtx.lock();
		Span* span = PageCache::_idSpanMap.get(id);
		//pageCache._pageMtx.unlock();

		// 将当前节点放入对应span
		span->freeList.push_front(cur);

		// 如果span的被使用次数减为0，就向pagecache归还span
		if (--span->use_count == 0)
		{
			// 先在spanlists中移除该span
			_spanLists[index].erase(span);

			lk.unlock();
			PageCache& pageCache = PageCache::GetInstance();
			pageCache.ReleaseSpanToPageCache(span);
			lk.lock();
		}
		
		cur = next;
	}
}
