# pools
Simple memory pool implementations

### Description
Sometimes you need to allocate a lot of small objects quickly. Maybe you're deserializing data into an object hierarchy. Maybe you're spawning entities that need to stick around over multiple frames. Maybe you have a very large linked list. If you're tired of poor spatial locality and tons of calls to malloc slowing you down, check out an object pool!

This library (pool.h) provides a simple object pool implementation. The pool allocates blocks of memory large enough to hold multiple objects of the requested type _T_, then doles out pointers to instances of _T_ allocated from those blocks on request. If a block is exhausted, the pool allocates a new block, growing geometrically by a configurable amount. The max block size is also configurable. The pool maintains a free list across and within blocks that reclaims destroyed objects. The user can also choose to release all the memory held by the pool at once without running destructors, making deallocation fast.

This library also provides a multipool implementation. The multipool is appropriate in situations where all types that need object pools are known at compile time. For instance, a `Multipool<A, B, C>` holds a `std::tuple<Pool<A>, Pool<B>, Pool<C>>` and dispatches requests for instances of `A`, `B`, and `C` to the appropriate pool. Each contained pool grows independently. The benefit of this variant of multipool is that no space is wasted; only the necessary pools are instantiated, and there is no wasted memory due to fitting objects in the nearest arbitrarily-sized pool.

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
