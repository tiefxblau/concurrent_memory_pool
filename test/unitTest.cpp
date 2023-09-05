#include <vector>
#include <ctime>
#include <thread>

#include "../concurrentPool.h"

struct A
{
	int val = 10;
	A* next = nullptr;
	std::vector<int> arr;

	A()
		: arr(16)
	{}
};

void test1()
{
	const int kRounds = 5;
	
	const int kTestTimes = 100000;

	std::vector<A*> arr(kTestTimes);

	clock_t begin1 = clock();
	for (int i = 0; i < kRounds; ++i)
	{
		for (int j = 0; j < kTestTimes; ++j)
		{
			arr[j] = new A;
		}

		for (int j = 0; j < kTestTimes; ++j)
		{
			delete arr[j];
		}
	}
	clock_t end1 = clock();

	ObjectPool<A> op;
	clock_t begin2 = clock();
	for (int i = 0; i < kRounds; ++i)
	{
		for (int j = 0; j < kTestTimes; ++j)
		{
			arr[j] = op.New();
		}

		for (int j = 0; j < kTestTimes; ++j)
		{
			op.Delete(arr[j]);
		}
	}
	clock_t end2 = clock();

	cout << (end1 - begin1) << endl;
	cout << (end2 - begin2) << endl;
}

void test2()
{
	int times = 10000000;
	int size = 1333;
	int align = 128;
	int shift = 7;

	//int begin1 = clock();
	/*for (int i = 0; i < times; ++i)
	{
		int a = (size + align - 1) & ~(align - 1);
	}
	int end1 = clock();

	int begin2 = clock();
	for (int i = 0; i < times; ++i)
	{
		int a = (size + align - 1) >> shift;
	}
	int end2 = clock();*/

	/*cout << (end1 - begin1) << endl;
	cout << (end2 - begin2) << endl;*/

	for (int i = 1; i <= 128; ++i)
	{
		int size = i;
		int align = 8;
		int align_shift = 3;
		size = (size + align - 1) & ~(align - 1);
		int b = ((size + (1 << align_shift) - 1) >> align_shift) - 1;
		int c =  (size - 1) >> align_shift;

		if (b != c)
		{
			cout << b << " " << c << endl;
		}
	}
	for (int i = 129; i <= 1024; ++i)
	{
		int size = i;
		int align = 16;
		int align_shift = 4;
		size = (size + align - 1) & ~(align - 1);
		int b = ((size + (1 << align_shift) - 1) >> align_shift) - 1;
		int c = (size - 1) >> align_shift;

		if (b != c)
		{
			cout << b << " " << c << endl;
		}
	}
	for (int i = 1025; i <= 1024 * 8; ++i)
	{
		int size = i;
		int align = 128;
		int align_shift = 7;
		size = (size + align - 1) & ~(align - 1);
		int b = ((size + (1 << align_shift) - 1) >> align_shift) - 1;
		int c = (size - 1) >> align_shift;

		if (b != c)
		{
			cout << b << " " << c << endl;
		}
	}
	for (int i = 1024 * 8 + 1; i <= 64 * 1024; ++i)
	{
		int size = i;
		int align = 1024;
		int align_shift = 10;
		size = (size + align - 1) & ~(align - 1);
		int b = ((size + (1 << align_shift) - 1) >> align_shift) - 1;
		int c = (size - 1) >> align_shift;

		if (b != c)
		{
			cout << b << " " << c << endl;
		}
	}
}

void Alloc(int bytes)
{
	if (pTLS_threadCache == nullptr)
	{
		pTLS_threadCache = new ThreadCache;
	}

	pTLS_threadCache->Allocate(bytes);
}

void ThreadRun(int bytes)
{
	for (int i = 0; i < 5000; ++i)
	{
		Alloc(bytes);
	}
}

void ThreadCacheTest()
{
	ThreadCache tc;
	std::thread t1 = std::thread(ThreadRun, 6);
	std::thread t2 = std::thread(ThreadRun, 7);

	t1.join();
	t2.join();
}

void ConcurrentTest()
{
	for (int j = 0; j < 10; ++j)
	{
		int times = 60;
		int begin1 = clock();
		std::vector<void*> ptrs(times);
		for (int i = 0; i < times; ++i)
		{
			//ptrs[i] = ConcurrentAlloc(i % 16);
			//ptrs[i] = ConcurrentAlloc(i % (64 * 1024));
			ptrs[i] = ConcurrentAlloc(8 * 1024 * i);
		}
		int end1 = clock();

		int begin2 = clock();
		for (int i = 0; i < times; ++i)
		{
			ConcurrentDealloc(ptrs[i]);
		}
		int end2 = clock();

		int begin3 = clock();
		for (int i = 0; i < times; ++i)
		{
			//ptrs[i] = malloc(i % (64 * 1024));
			ptrs[i] = malloc(8 * 1024 * i);
		}
		int end3 = clock();

		int begin4 = clock();
		for (int i = 0; i < times; ++i)
		{
			free(ptrs[i]);
		}
		int end4 = clock();
	}
}

void ConcurrentTest2()
{
	ThreadCache tc;
	int N = 8 * 1024;
	std::vector<void*> arr(N);
	for (int i = 0; i < N; ++i)
	{
		arr[i] = tc.Allocate(abs(rand()));
		*(int*)arr[i] = 0xff;
	}

	for (int i = 0; i < N; ++i)
	{
		tc.Deallocate(arr[i]);
	}
	void* ptr2 = tc.Allocate(2);

}

void ConcurrentTest3()
{
	ThreadCache tc;
	int N = 60;
	std::vector<void*> arr(N);
	for (int i = 0; i < N; ++i)
	{
		arr[i] = ConcurrentAlloc(5 * 1024 * 8 * i);
		*(int*)arr[i] = 0xff;
	}

	for (int i = 0; i < N; ++i)
	{
		ConcurrentDealloc(arr[i]);
	}
	void* ptr2 = tc.Allocate(2);

}

void MultiThreadTest()
{
	int tn = 4;
	std::vector<std::thread> vt(tn);
	for (int i = 0; i < tn; ++i)
	{
		vt[i] = std::thread(ConcurrentTest);
	}

	for (int i = 0; i < tn; ++i)
	{
		vt[i].join();
	}
}

int main()
{
	//ThreadCacheTest();
	//ConcurrentTest3();
	MultiThreadTest();

	return 0;
}