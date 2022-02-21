# pools
Simple memory pool implementations

### Motivation
Sometimes you need to allocate a lot of small objects quickly. Maybe you're deserializing data into an object hierarchy. Maybe you're spawning entities that need to stick around over multiple frames. Maybe you have a very large linked list. If you're tired of poor spatial locality and tons of calls to malloc slowing you down, check out an object pool!

### Description
This library (pool.h) provides a simple object pool implementation. The pool requests blocks of memory from the CRT allocator large enough to hold multiple objects of the requested type _T_, then doles out pointers to instances of _T_ allocated from those blocks on request. If a block is exhausted, the pool requests a new block, growing geometrically by a configurable amount. The max block size is also configurable. The pool maintains a free list across and within blocks that reclaims destroyed objects. The user can also choose to release all the memory held by the pool at once without running destructors, making deallocation fast.

This library also provides a multipool implementation. The multipool is appropriate in situations where all types that need object pools are known at compile time. For instance, a `Multipool<A, B, C>` holds a `std::tuple<Pool<A>, Pool<B>, Pool<C>>` and dispatches requests for instances of `A`, `B`, and `C` to the appropriate pool. Each contained pool grows independently. The benefit of this variant of multipool is that no space is wasted; only the necessary pools are instantiated, and there is no wasted memory due to fitting objects in the nearest arbitrarily-sized pool.

### Use
The following code snippet shows example use of the pool:

```c++
struct A {};
struct B {};
struct C {};

void demo()
{
    // Demonstrate use of a Pool<T>
    {
        // Create a pool of 'A's with an initial block of 64 entries.
        Pool<A> a_pool(64);

        // Can be called 64 times before triggering another allocation.
        A* a1 = a_pool.construct();
        A* a2 = a_pool.construct();

        // Release the memory to the pool. This does not call free() -
        // it returns the space allocated for `a` to the pool.
        a_pool.destroy(a);
        
        // Release all memory managed by `a_pool`. Happens on destruction.
        // It is OK to not manually destroy `a2` if its destructor is trivial.
        a_pool.release();
    }
    
    // Demonstrate use of a Multipool<...Ts>
    {
        // Create a multipool of 'A's, 'B's, and 'C's.
        // Each subpool has initial block size of 64.
        Multipool<A,B,C> abc_pool(64);
        
        // Can create 64 (each) A, B, C objects before requesting more memory.
        A* a = abc_pool.construct<A>();
        B* b = abc_pool.construct<B>();
        C* c = abc_pool.construct<C>();

        // int* x = abc_pool.construct<int>(); // Compile error! `abc_pool` does not pool this type.
        
        // Can destroy these objects - does not call free().
        abc_pool.destroy(a);
        
        // Release all memory managed by `abc_pool`. Happens on destruction.
        // It is OK to not manually destroy `b` and `c` if their destructors are trivial.
        abc_pool.release_all();
    }
}
```

### Testing
This repository contains a small set of tests of `Pool` and `Multipool`. It evaluates performance allocating many objects of a given type at once, then releasing them. It also evaluates a more-realistic scenario where the program creates and destroys objects of various types in a pseudo-random pattern. In all of these cases, the pool allocators come out ahead. They perform worse as object sizes grow.

Sample test output on my laptop's i5-8250 CPU @ 1.6GHz, running on WSL2:

```
Time to allocate, free 1000000 objects of size 16:
    Pooled: 18226500 ticks
Individual: 52356900 ticks
Time to allocate, free 1000000 objects of size 40:
    Pooled: 31536100 ticks
Individual: 58437100 ticks
Time to allocate, free 1000000 objects of size 72:
    Pooled: 50715800 ticks
Individual: 70661400 ticks
Time to allocate, free 1000000 objects of size 136:
    Pooled: 94071500 ticks
Individual: 103757600 ticks
Time to allocate, free 1000000 * 4 objects (mixed alloc/free):
    Pooled: 26612500 ticks
Individual: 38116600 ticks
```
