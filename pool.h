#pragma once

#include <cassert>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <memory>

constexpr bool DEBUG_PRINT = false;

// An object pool for a particular type. Stores blocks of memory to be doled
// out as requested via the construct function. The destroy function frees the
// given memory and allows memory reuse. When a memory block is exhausted, the
// pool allocates a new block of greater size, as determined by the given growth
// factor. Destroying pointers takes O(1) time. The release function allows the
// user to return all memory to the upstream allocator without running destructors.
// The free list spans all blocks managed by the pool.
template <typename T, size_t GrowthFactor = 2, size_t MaxBlockSize = 1024>
class Pool
{
public:
    using type = T;
    using pointer = T*;

    Pool(size_t size = 1)
        : m_blocks()
        , m_blockSize(size)
        , m_nextFree(nullptr)
    {
        assert(size > 0); // Pool must hold at least one object to start.
        assert(size <= MaxBlockSize); // Block must not exceed max block size.

        m_blocks.push_back(std::make_unique<Item[]>(size));

        if constexpr (DEBUG_PRINT)
        {
            std::cout << "Allocating " << sizeof(Item) << " (obj size) * "
                      << size << " (block size) = " << sizeof(Item) * size << " bytes\n";
        }

        auto& firstBlock = m_blocks.front();
        for (size_t i = 1; i < size; ++i)
            firstBlock[i-1].m_next = &firstBlock[i];

        m_nextFree = &firstBlock[0];
    }

    // Non-copyable
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    // Movable
    Pool(Pool&&) = default;
    Pool& operator=(Pool&&) = default;

    template <typename ...Ts>
    [[nodiscard]] pointer construct(Ts&& ...args)
    {
        return new (allocate()) type(std::forward<Ts>(args)...);
    }

    void destroy(pointer p)
    {
        if (p == nullptr)
            return;

        p->~type();
        deallocate(p);
    }

    // Deallocate every block! Doesn't run destructors.
    void release()
    {
        m_blocks.clear();
        m_nextFree = nullptr;
    }

    bool full() const { return m_nextFree == nullptr; }

    void print() const
    {
        size_t freeCount = 0;
        Item* itemPtr = m_nextFree;
        while (itemPtr != nullptr)
        {
            freeCount++;
            itemPtr = itemPtr->m_next;
        }

        std::cout << "Element size: " << sizeof(type) << "\n";
        std::cout << "Element alignment: " << alignof(type) << "\n";
        std::cout << "Next free: " << (void*)m_nextFree << "\n";
        std::cout << "Block size: " << m_blockSize << "\n";
        std::cout << "Free count: " << freeCount << "\n";

        std::cout << std::hex << std::setfill('0');
        for (auto& block : m_blocks)
        {
            std::cout << "\nBlock start: " << block.get() << "\n";
            for (size_t i = 0; i < m_blockSize; ++i)
            {
                const pointer p = std::launder(reinterpret_cast<pointer>(&block[i]));
                const std::byte* p_bytes = reinterpret_cast<std::byte*>(p);
                for (int j = sizeof(type) - 1; j >= 0; --j)
                {
                    if (j == sizeof(type) - 1)
                        std::cout << (void*)p_bytes << ": ";

                    std::cout << std::setw(2) << static_cast<unsigned>(p_bytes[j]) << " ";

                    if (j == 0)
                        std::cout << "\n";
                }
            }
        }
    }

private:
    [[nodiscard]] pointer allocate()
    {
        // Out of space - allocate new block!
        if (m_nextFree == nullptr)
        {
            // Grow blocks sizes by the growth factor each time.
            // Do not allow the block size to be greater than the max block size.
            m_blockSize = std::min(GrowthFactor * m_blockSize, MaxBlockSize);

            if constexpr (DEBUG_PRINT)
            {
                std::cout << "Allocating " << sizeof(Item) << " (obj size) * "
                          << m_blockSize << " (block size) = "
                          << sizeof(Item) * m_blockSize << " bytes\n";
            }

            auto& newBlock = m_blocks.emplace_back(std::make_unique<Item[]>(m_blockSize));
            for (size_t i = 1; i < m_blockSize; ++i)
                newBlock[i-1].m_next = &newBlock[i];

            m_nextFree = &newBlock[0];
        }

        Item* freeItem = m_nextFree;
        m_nextFree = freeItem->m_next;
        return std::launder(reinterpret_cast<pointer>(&freeItem->m_storage));
    }

    void deallocate(pointer p) noexcept
    {
        Item* item = reinterpret_cast<Item*>(p);
        item->m_next = m_nextFree;
        m_nextFree = item;
    }

    union Item
    {
        std::aligned_storage_t<sizeof(T), alignof(T)> m_storage;
        Item* m_next;

        Item() : m_next(nullptr) {}
    };

    std::vector<std::unique_ptr<Item[]>> m_blocks;
    size_t m_blockSize;
    Item* m_nextFree;
};

// Given a list of types, the Multipool stores a tuple of Pools of all given types.
// Request objects of any type in the given type list from the pool, and the Multipool
// will dispatch that request to the relevant pool. This is helpful if all types that
// need pooling are known at compile time, and avoids wasted memory by sizing member
// pools to the objects exactly.
template <typename ...Ts>
class Multipool
{
public:
    Multipool(size_t n)
        : pools(Pool<Ts>{n}...)
    {}

    // Create a T* from a pool. Allocates a new block from the upstream allocator if necessary.
    template <typename T, typename ...Args>
    auto construct(Args&& ...args)
    {
        return std::get<Pool<T>>(pools).construct(std::forward<Args>(args)...);
    }

    // Destroys the given T* and deallocates its memory from the relevant pool.
    template <typename T>
    void destroy(T* p)
    {
        std::get<Pool<T>>(pools).destroy(p);
    }

    // Deallocates all backing memory for the given pool type. Does not run destructors!
    template <typename T>
    void release()
    {
        std::get<Pool<T>>(pools).release();
    }

    // Deallocates all backing memory for all pools. Does not run destructors!
    void release_all()
    {
        std::apply([](auto&& ...pool){((pool.release()), ...);}, pools);
    }

    template <typename T>
    Pool<T>& get()
    {
        return std::get<Pool<T>>(pools);
    }

    Multipool(const Multipool&) = delete;
    Multipool(Multipool&&) = delete;
    Multipool& operator=(Multipool&&) = delete;
    Multipool& operator=(const Multipool&) = delete;

private:
    std::tuple<Pool<Ts>...> pools;
};

