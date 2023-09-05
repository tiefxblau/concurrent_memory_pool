#include "common.h"
#include "threadCache.h"
#include "centralCache.h"
#include "pageCache.h"

void* ThreadCache::Allocate(std::size_t bytes)
{
	if (bytes == 0)
		bytes = 1;

	// 找到目标自由链表（下标）
	std::size_t realBytes = SizeClass::RoundUp(bytes);
	std::size_t i = SizeClass::Index(realBytes);

	// 若自由链表为空, 从central cache中申请
	if (_freeLists[i].empty())
	{
		return FetchFromCentralCache(realBytes, i);
	}
	else
	{
		return _freeLists[i].pop_front();
	}
}

void ThreadCache::Deallocate(void* ptr)
{
	if (ptr == nullptr)
		return;
	
	std::size_t id = (std::size_t)ptr >> kPageShift;

	//PageCache& pageCache = PageCache::GetInstance();
	//pageCache._pageMtx.lock();
	std::size_t bytes = PageCache::_idSpanMap.get(id)->obj_size;
	//pageCache._pageMtx.unlock();

	std::size_t i = SizeClass::Index(bytes);


	// 归还到自由链表中，若链表长度大于最大申请数量就继续向central cache归还
	_freeLists[i].push_front(ptr);

	if (_freeLists[i].size() > _maxGetSize)
	{
		ListTooLong(i);
	}
}

void* ThreadCache::FetchFromCentralCache(std::size_t bytes, std::size_t index)
{
	// 预期可拿到的内存块数量
	std::size_t fetchNum = (std::min)(_maxGetSize, SizeClass::NumOfMoveSize(bytes));
	// _maxGetSize采用慢增长策略
	if (_maxGetSize == fetchNum)
		IncreaseGetSize();

	void* begin = nullptr, * end = nullptr;
	CentralCache& centralIns = CentralCache::GetInstance();
	// fetchNum更新为实际数量
	fetchNum = centralIns.FetchRange(begin, end, fetchNum, index, bytes);

	if (fetchNum == 1)
	{
		return begin;
	}
	else
	{
		// 若拿到多于一个内存块，则将多的内存块放入自由链表桶对应自由链表中
		void* res = begin;
		begin = FreeList_next(begin);
		_freeLists[index].push_front(begin, end, fetchNum - 1);
		return res;
	}

}

void ThreadCache::ListTooLong(std::size_t index)
{
	// 一次归还的数量
	std::size_t batchNum = _maxGetSize;

	// 拿到归还部分的头尾
	void* head, * tail = nullptr;
	_freeLists[index].pop_front(head, tail, batchNum);

	CentralCache& central = CentralCache::GetInstance();
	// 向central cache归还
	central.ReleaseToSpans(head, tail, index);
}

