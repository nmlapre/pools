#include "pool.h"

#include <chrono>

static constexpr size_t pool_init_block_size = 8;
static constexpr size_t n_iterations = 1000000;

// Simple RAII timer to time tests.
struct Timer
{
    Timer(const char* label)
        : m_label(label)
        , m_startTime(std::chrono::steady_clock::now())
    {}

    ~Timer()
    {
        auto diff = std::chrono::steady_clock::now() - m_startTime;
        std::cout << std::setw(12) << m_label << diff.count() << " ticks\n";
    }

    const char* m_label;
    std::chrono::time_point<std::chrono::steady_clock> m_startTime;
};

// Test types: imagine some inheritance hierarchy that needs pooling.
struct Base { virtual ~Base() = default; };
struct A : Base { std::byte data[8];   };
struct B : Base { std::byte data[32];  };
struct C : Base { std::byte data[64];  };
struct D : Base { std::byte data[128]; };

using DataMultipool = Multipool<A, B, C, D>;

namespace MultipoolInstance
{
    DataMultipool& get()
    {
        static DataMultipool mp(pool_init_block_size);
        return mp;
    }
}

using base_ptr = std::unique_ptr<Base, std::function<void(Base*)>>;

template <typename T>
void mp_deleter(Base* p)
{
    MultipoolInstance::get().destroy(static_cast<T*>(p));
}

template <typename T>
void new_deleter(Base* p)
{
    delete static_cast<T*>(p);
}

template <typename T, typename ...Args>
base_ptr makeBasePtrMP(Args&& ...args)
{
    DataMultipool& mp = MultipoolInstance::get();
    return { mp.construct<T>(std::forward<Args>(args)...), mp_deleter<T> };
}

template <typename T, typename ...Args>
base_ptr makeBasePtrCRT(Args&& ...args)
{
    return { new T(std::forward<Args>(args)...), new_deleter<T> };
}

template <typename T>
void MassAllocPool()
{
    Pool<T> pool(pool_init_block_size);
    std::vector<Base*> ptrs;
    ptrs.reserve(n_iterations);

    for (size_t i = 0; i < n_iterations; ++i)
        ptrs.emplace_back(pool.construct());

    for (size_t i = 0; i < n_iterations; ++i)
        pool.destroy(static_cast<T*>(ptrs[i]));
}

template <typename T>
void MassAllocCRT()
{
    std::vector<Base*> ptrs;
    ptrs.reserve(n_iterations);

    for (size_t i = 0; i < n_iterations; ++i)
        ptrs.emplace_back(new T());

    for (size_t i = 0; i < n_iterations; ++i)
        delete ptrs[i];
}

// Allocate and delete from the multipool based on an array of clamped random numbers.
// This is meant to simulate a more realistic workload with interspersed alloc, delete.
void MixedPoolAlloc(const std::array<int, n_iterations>& randomVals)
{
    DataMultipool& mp = MultipoolInstance::get();

    std::vector<base_ptr> ptrs;
    ptrs.reserve(n_iterations);

    for (size_t i = 0; i < n_iterations; ++i)
    {
        switch (randomVals[i])
        {
            case 0:
                ptrs.push_back(makeBasePtrMP<A>());
                break;
            case 1:
                ptrs.push_back(makeBasePtrMP<B>());
                break;
            case 2:
                ptrs.push_back(makeBasePtrMP<C>());
                break;
            case 3:
                ptrs.push_back(makeBasePtrMP<D>());
                break;
            default:
                if (ptrs.size() > 4)
                {
                    ptrs.pop_back();
                    ptrs.pop_back();
                    ptrs.pop_back();
                    ptrs.pop_back();
                }
                break;
        }
    }
}

void MixedCRTAlloc(const std::array<int, n_iterations>& randomVals)
{
    std::vector<base_ptr> ptrs;
    ptrs.reserve(n_iterations);

    for (size_t i = 0; i < n_iterations; ++i)
    {
        switch (randomVals[i])
        {
            case 0:
                ptrs.push_back(makeBasePtrCRT<A>());
                break;
            case 1:
                ptrs.push_back(makeBasePtrCRT<B>());
                break;
            case 2:
                ptrs.push_back(makeBasePtrCRT<C>());
                break;
            case 3:
                ptrs.push_back(makeBasePtrCRT<D>());
                break;
            default:
                if (ptrs.size() > 4)
                {
                    ptrs.pop_back();
                    ptrs.pop_back();
                    ptrs.pop_back();
                    ptrs.pop_back();
                }
                break;
        }
    }
}

template <typename T>
void TestMassAlloc()
{
    std::cout << "Time to allocate, free " << n_iterations << " objects of size " << sizeof(T) << ":\n";

    // Test mass allocation of T, Pool allocator
    {
        Timer timer("Pooled: ");
        MassAllocPool<T>();
    }

    // Test mass allocation of T, CRT allocator
    {
        Timer timer("Individual: ");
        MassAllocCRT<T>();
    }
}

void TestMixedAlloc()
{
    // Set up an array of random values within [0,4] to simulate real alloc/delete requests
    srand(time(0));
    std::array<int, n_iterations> randomVals;
    for (size_t i = 0; i < n_iterations; ++i)
        randomVals[i] = rand() % 5;

    std::cout << "Time to allocate, free " << n_iterations << " * 4 objects (mixed alloc/free):\n";

    // Test mixed allocations/deletions, multipool
    {
        Timer timer("Pooled: ");
        MixedPoolAlloc(randomVals);
        MultipoolInstance::get().release_all();
    }

    // Test mixed allocations/deletions, CRT allocator
    {
        Timer timer("Individual: ");
        MixedCRTAlloc(randomVals);
    }
}

int main()
{
    // Test mass allocation then deallocation of various object sizes.
    // Exercises the Pool class.
    TestMassAlloc<A>();
    TestMassAlloc<B>();
    TestMassAlloc<C>();
    TestMassAlloc<D>();

    // Test pseudo-random mixed allocation/deallocation of multiple object types.
    // Exercises the Multipool class.
    TestMixedAlloc();

    return 0;
}

