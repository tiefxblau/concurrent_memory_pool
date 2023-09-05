#include "../concurrentPool.h"
#include <thread>
#include <atomic>

#include <vector>

void BenchMarkTest(int rounds, int works, int times)
{
	std::atomic<size_t> tcCost(0);
	std::atomic<size_t> tcAllocCost(0), tcDeallocCost(0);

	std::atomic<size_t> cost(0);
	std::atomic<size_t> allocCost(0), deallocCost(0);

	std::vector<std::thread> threads(works);

	for (auto& t : threads)
	{
		t = std::thread([&, times]() {
			std::vector<void*> ptrs(times);

			for (int j = 0; j < rounds; ++j)
			{
				int begin1 = clock();
				for (int i = 0; i < times; ++i)
				{
					// ptrs[i] = ConcurrentAlloc(i % 20);
					// ptrs[i] = ConcurrentAlloc(i % (64 * 1024));
					ptrs[i] = ConcurrentAlloc(8 * 1024 * i);
				}
				int end1 = clock();

				int begin2 = clock();
				for (int i = 0; i < times; ++i)
				{
					ConcurrentDealloc(ptrs[i]);
				}
				int end2 = clock();

				tcCost += end1 - begin1 + end2 - begin2;
				tcAllocCost += end1 - begin1;
				tcDeallocCost += end2 - begin2;

				int begin3 = clock();
				for (int i = 0; i < times; ++i)
				{
					// ptrs[i] = malloc(i % 20);
					// ptrs[i] = malloc(i % (64 * 1024));
					ptrs[i] = malloc(8 * 1024 * i);
				}
				int end3 = clock();

				int begin4 = clock();
				for (int i = 0; i < times; ++i)
				{
					free(ptrs[i]);
				}
				int end4 = clock();

				cost += end3 - begin3 + end4 - begin4;
				allocCost += end3 - begin3;
				deallocCost += end4 - begin4;
			}

		});
	}

	for (auto& t : threads)
	{
		t.join();
	}


	printf("%u个线程并发执行%u轮次, 每轮次ConcurrentAlloc%u次, 花费: %u ms\n", works, rounds, times, int(tcAllocCost));
	printf("                      , 每轮次ConcurrentDealloc%u次, 花费: %u ms\n", times, int(tcDeallocCost));
	printf("                      , 共花费: %u ms\n\n", int(tcCost));

	printf("%u个线程并发执行%u轮次, 每轮次malloc%u次, 花费: %u ms\n", works, rounds, times, int(allocCost));
	printf("                      , 每轮次free%u次, 花费: %u ms\n", times, int(deallocCost));
	printf("                      , 共花费: %u ms\n", int(cost));
}

int main()
{
	BenchMarkTest(100, 4, 2560);
	return 0;
}