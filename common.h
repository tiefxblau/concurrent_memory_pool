#pragma once
#include <cstring>

#include <iostream>
#include <cassert>
#include <mutex>
#include <algorithm>

#include <unordered_map>

#include "alloc.h"
using std::cout;
using std::endl;

// 返回node指向的下一节点
static void*& FreeList_next(void* node)
{
	return *(void**)node;
}

// 自由链表
class FreeList
{
private:
	void* _head = nullptr;
	size_t _size = 0;

public:
	void* begin() const
	{
		return _head;
	}

	std::size_t size() const
	{
		return _size;
	}

	void push_front(void* inNode)
	{
		if (_head == nullptr)
		{
			_head = inNode;
			FreeList_next(inNode) = nullptr;
		}
		else
		{
			FreeList_next(inNode) = _head;
			_head = inNode;
		}
		
		++_size;
	}

	// 从头部插入begin到end的freelist
	void push_front(void* begin, void* end, std::size_t len)
	{
		if (_head == nullptr)
		{
			_head = begin;
			FreeList_next(end) = nullptr;
		}
		else
		{
			FreeList_next(end) = _head;
			_head = begin;
		}
		_size += len;
	}

	void* pop_front()
	{
		assert(_head);

		void* tmp = _head;
		_head = FreeList_next(_head);
		--_size;

		FreeList_next(tmp) = nullptr;
		return tmp;
	}

	// 删除从头部到end的节点
	void pop_front(void* end, std::size_t len)
	{
		assert(_head);

		_head = FreeList_next(end);
		_size -= len;

		FreeList_next(end) = nullptr;
	}

	// 删除从头部开始的len个节点，输出参数head，tail
	void pop_front(void*& head, void*& tail, std::size_t len)
	{
		assert(_head);
		assert(len <= size());

		head = tail = _head;
		for (int i = 0; i < len - 1; ++i)
		{
			tail = FreeList_next(tail);
		}

		_head = FreeList_next(tail);
		_size -= len;

		FreeList_next(tail) = nullptr;
	}

	bool empty() const
	{
		return _head == nullptr;
	}

	// 替换头节点
	void ReplaceHead(void* newHead, std::size_t len)
	{
		_head = newHead;
		_size = len;
	}
};

// 自由链表桶大小
static const std::size_t kNFreeList = 184;

// page cache中spanlist的大小
static const std::size_t kNPageList = 128 + 1;

// page大小对应的2的次方
static const std::size_t kPageShift = 13;

// 可在三层缓存中执行申请释放操作的最大内存
static const std::size_t kMaxBytes = 64 * 1024;


// 管理对齐和映射等关系
class SizeClass
{
public:
	// 控制在1%-12%左右的内碎片浪费
	// [1,128]					8byte对齐	     freelist[0,16)
	// [129,1024]				16byte对齐		 freelist[16,72)
	// [1025,8*1024]			128byte对齐	     freelist[72,128)
	// [8*1024+1,64*1024]		1024byte对齐     freelist[128,184)

	// 返回对齐后的大小
	static std::size_t RoundUp(std::size_t size)
	{
		assert(size > 0);
		
		if (size <= 128)
		{
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{
			return  _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{
			return  _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{
			return  _RoundUp(size, 1024);
		}
		else
		{
			return _RoundUp(size, 1 << kPageShift);
		}
	}

	// 将内存大小映射为自由链表桶的下标
	static std::size_t Index(std::size_t size)
	{
		assert(size <= kMaxBytes && size > 0);

		if (size <= 128)
		{
			return _Index(size, 3);
		}
		else if (size <= 1024)
		{
			return  _Index(size - 128, 4) + 16;
		}
		else if (size <= 8 * 1024)
		{
			return  _Index(size - 1024, 7) + 72;
		}
		else if (size <= 64 * 1024)
		{
			return  _Index(size - 1024 * 8, 10) + 128;
		}
		else
		{
			assert(false);
			return 0;
		}
	}

	// 一次向系统申请的页数
	static std::size_t NumOfMovePage(std::size_t bytes)
	{
		std::size_t moveSize = NumOfMoveSize(bytes);
		std::size_t pages = (moveSize * bytes) >> kPageShift;

		if (pages == 0)
			pages = 1;

		return pages;
	}

	// 可从central cache中获取的内存块的数量
	static std::size_t NumOfMoveSize(std::size_t bytes)
	{
		if (bytes == 0)
			return 0;

		// [2, 512]
		std::size_t num = kMaxBytes / bytes;
		if (num < 2)
			num = 2;

		if (num > 512)
			num = 512;

		return num;
	}


private:

	static std::size_t _Index(std::size_t size, int align_shift)
	{
		//return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
		return (size - 1) >> align_shift;
	}

	static std::size_t _RoundUp(std::size_t size, int align)
	{
		return (size + align - 1) & ~(align - 1);
	}
};

// 管理一个跨度的大块内存
struct Span
{
	// 最小的页号
	std::size_t page_id = 0;
	// 包含的页数
	std::size_t page_num = 0;

	Span* next = nullptr;
	Span* prev = nullptr;

	FreeList freeList;
	// freeList的使用次数
	std::size_t use_count = 0;
	//  当span被central cache获取即视为被使用
	bool is_used = false;

	// 当前span对应内存所存储的对象的大小
	std::size_t obj_size = 0;
};

// 定长内存池，用于代替new
static ObjectPool<Span> spanPool;

// 双向循环带头链表
class SpanList
{
public:
	SpanList()
		: _head(spanPool.New())
	{
		_head->next = _head;
		_head->prev = _head;
	}

	// 获取一个非空的span
	Span* GetOneSpan() const
	{
		Span* cur = _head->next;
		while (cur != _head)
		{
			if (!cur->freeList.empty())
				return cur;

			cur = cur->next;
		}

		return nullptr;
	}

	void push_front(Span* inNode)
	{
		assert(inNode);
		Span* next = _head->next;

		_head->next = inNode;
		inNode->prev = _head;

		next->prev = inNode;
		inNode->next = next;
	}

	void push_back(Span* inNode)
	{
		assert(inNode);
		Span* prev = _head->prev;

		prev->next = inNode;
		inNode->next = _head;

		_head->prev = inNode;
		inNode->prev = prev;
	}

	void erase(Span* pos)
	{
		assert(pos && pos != _head);
		Span* next = pos->next;
		Span* prev = pos->prev;

		prev->next = next;
		next->prev = prev;
	}

	Span* pop_front()
	{
		Span* first = _head->next;
		erase(first);
		return first;
	}

	bool empty() const
	{
		return _head->next == _head;
	}

private:
	Span* _head;

public:
	// 当多个线程向桶的同一个SpanList申请内存时, 需要加锁
	std::mutex _mtx;
};
