#pragma once
#include "common.h"

static constexpr std::size_t kAllocSize = 1024 * 1024;

// Two-level radix tree
template <int BITS>
class PageMap2 {
private:
	// The leaf node (regardless of pointer size) always maps 2^15 entries;
  // with 8K pages, this gives us 256MB mapped per leaf node.
	static constexpr int kLeafBits = 15;
	static constexpr int kLeafLength = 1 << kLeafBits;
	static constexpr int kRootBits = (BITS >= kLeafBits) ? (BITS - kLeafBits) : 0;

	static_assert(kRootBits < sizeof(int) * 8 - 1, "kRootBits is too large");
	static constexpr int kRootLength = 1 << kRootBits;

	// Leaf node
	struct Leaf {
		Span* values[kLeafLength];
	};

	Leaf* root_[kRootLength];             // Pointers to 32 child nodes

public:
	typedef uintptr_t Number;

	//explicit TCMalloc_PageMap2(void* (*allocator)(size_t)) {
	PageMap2() {
		//allocator_ = allocator;
		memset(root_, 0, sizeof(root_));

		PreallocateMoreMemory();
	}

	Span* get(Number k) const {
		const Number i1 = k >> kLeafBits;
		const Number i2 = k & (kLeafBits - 1);
		if ((k >> BITS) > 0 || root_[i1] == nullptr) {
			return nullptr;
		}
		return root_[i1]->values[i2];
	}

	void set(Number k, Span* v) {
		const Number i1 = k >> kLeafBits;
		const Number i2 = k & (kLeafBits - 1);
		assert(i1 < kRootLength && i2 < kLeafBits);
		root_[i1]->values[i2] = v;
	}

private:

	bool Ensure(Number start, size_t n) {
		for (Number key = start; key <= start + n - 1;) {
			const Number i1 = key >> kLeafLength;

			// Check for overflow
			if (i1 >= kRootLength)
				return false;

			// Make 2nd level node if necessary
			if (root_[i1] == nullptr) {
				//Leaf* leaf = reinterpret_cast<Leaf*>((*allocator_)(sizeof(Leaf)));
				//if (leaf == NULL) return false;
				static ObjectPool<Leaf>	leafPool(kAllocSize);
				Leaf* leaf = (Leaf*)leafPool.New();

				memset(leaf, 0, sizeof(*leaf));
				root_[i1] = leaf;
			}

			// Advance key past whatever is covered by this leaf node
			key = ((key >> kLeafBits) + 1) << kLeafBits;
		}
		return true;
	}

	void PreallocateMoreMemory() {
		// Allocate enough to keep track of all possible pages
		Ensure(0, 1 << BITS);
	}
};

// Three-level radix tree
template <int BITS>
class PageMap3 {
private:
	
	static constexpr int kLeafBits = (BITS + 2) / 3;  // Round up
	static constexpr int kLeafLength = 1 << kLeafBits;
	static constexpr int kMidBits = (BITS + 2) / 3;  // Round up
	static constexpr int kMidLength = 1 << kMidBits;
	static constexpr int kRootBits = BITS - kLeafBits - kMidBits;
	static_assert(kRootBits > 0, "Too many bits assigned to leaf and mid");
	// (1<<kRootBits) must not overflow an "int"
	static_assert(kRootBits < sizeof(int) * 8 - 1, "Root bits too large");
	static constexpr int kRootLength = 1 << kRootBits;

	// Leaf node
	struct Leaf {
		Span* values[kLeafLength];
	};

	Leaf* NewLeaf() const
	{
		static ObjectPool<Leaf> leafPool(kAllocSize) ;
		return (Leaf*)leafPool.New();
	}

	struct Node {
		Leaf* leaves[kMidLength];

		Node() {
			memset(leaves, 0, sizeof(leaves));
		}
	};

	Node* NewNode() const
	{
		static ObjectPool<Node> nodePool(kAllocSize);
		return (Node*)nodePool.New();
	}

	Node* root_[kRootLength];             // Pointers to 32 child nodes

public:
	typedef uintptr_t Number;

	//explicit TCMalloc_PageMap2(void* (*allocator)(size_t)) {
	PageMap3() {
		//allocator_ = allocator;
		memset(root_, 0, sizeof(root_));

		//PreallocateMoreMemory();
	}

	Span* get(Number k) const {
		const Number i1 = k >> (kLeafBits + kMidBits);
		const Number i2 = (k >> kLeafBits) & (kMidLength - 1);
		const Number i3 = k & (kLeafLength - 1);
		/*assert(root_[i1] && root_[i1]->leaves[i2]);*/

		if ((k >> BITS) > 0 || root_[i1] == nullptr ||
			root_[i1]->leaves[i2] == nullptr) {
			return nullptr;
		}
		return root_[i1]->leaves[i2]->values[i3];
	}

	void set(Number k, Span* s) {
		assert(k >> BITS == 0);
		const Number i1 = k >> (kLeafBits + kMidBits);
		const Number i2 = (k >> kLeafBits) & (kMidLength - 1);
		const Number i3 = k & (kLeafLength - 1);
		if (root_[i1] == nullptr)
		{
			root_[i1] = NewNode();
		}
		if (root_[i1]->leaves[i2] == nullptr)
		{
			root_[i1]->leaves[i2] = NewLeaf();
		}
		root_[i1]->leaves[i2]->values[i3] = s;
	}

private:

	//bool Ensure(Number start, size_t n) {
	//	for (Number key = start; key <= start + n - 1;) {
	//		const Number i1 = key >> kLeafLength;

	//		// Check for overflow
	//		if (i1 >= kRootLength)
	//			return false;

	//		// Make 2nd level node if necessary
	//		if (root_[i1] == nullptr) {
	//			//Leaf* leaf = reinterpret_cast<Leaf*>((*allocator_)(sizeof(Leaf)));
	//			//if (leaf == NULL) return false;
	//			static ObjectPool<Leaf>	leafPool;
	//			Leaf* leaf = (Leaf*)leafPool.New();

	//			memset(leaf, 0, sizeof(*leaf));
	//			root_[i1] = leaf;
	//		}

	//		// Advance key past whatever is covered by this leaf node
	//		key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
	//	}
	//	return true;
	//}

	//void PreallocateMoreMemory() {
	//	// Allocate enough to keep track of all possible pages
	//	Ensure(0, 1 << BITS);
	//}
};
