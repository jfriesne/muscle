# muscle::RefCountable class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RefCountable.html)

```#include "util/RefCount.h"```

Reference-counting for heap-allocated objects, to makes memory-leaks and use-after-free errors nearly impossible.

* Similar to: [std::shared_ptr&lt;T&gt;](http://en.cppreference.com/w/cpp/memory/shared_ptr), [boost::intrusive_ptr&lt;T&gt;](https://www.boost.org/doc/libs/1_60_0/libs/smart_ptr/intrusive_ptr.html), [QSharedPointer](http://doc.qt.io/qt-5/qsharedpointer.html)
* Object to be reference-counted must be a subclass of [RefCountable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RefCountable.html).
* The ref-counted object will always be deleted by the destructor of the last [Ref](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Ref.html) that points to it -- explicitly calling `delete` is NEVER necessary!
* Often used in conjunction with the [ObjectPool](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ObjectPool.html) class to recycle used objects (to minimize heap allocations/deallocations)
* Idiom:  For any [RefCountable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RefCountable.html) class `FooBar`, the [DECLARE_REFTYPES()](https://public.msli.com/lcs/muscle/html/RefCount_8h.html#a5f9b4b0acbe24ff62f3cfddaa4b01d88) macro defines typedefs `FooBarRef` and `ConstFooBarRef`:

```
    class FooBar : public RefCountable 
    {
       [...]
    };
    DECLARE_REFTYPES(FooBar);  // macro to define FooBarRef and ConstFooBarRef types

    FooBarRef oneRef(new FooBar);  // same as Ref<FooBar> oneRef(new FooBar);
    ConstFooBarRef twoRef(new FooBar);  // same as ConstRef<FooBar> twoRef(new FooBar);

    twoRef()->Hello();  // () operator gives pointer to RefCountable object
```
* [Ref](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Ref.html) is similar to a read/write shared-pointer, [ConstRef](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ConstRef.html) is similar to a read-only shared-pointer.
* [RefCountable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RefCountable.html) uses an [AtomicCounter](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AtomicCounter.html) internally for its ref-count, and [ObjectPools](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ObjectPool.html) are thread-safe, so it's safe to pass [Ref](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Ref.html) and [ConstRef](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ConstRef.html) objects around to different threads.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/refcount` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [refcount/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/refcount/example_1_basic_usage.cpp)
* [refcount/example_2_with_object_pool.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/refcount/example_2_with_object_pool.cpp)
* [refcount/example_3_conversions.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/refcount/example_3_conversions.cpp)
* [refcount/example_4_dummy_refs.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/refcount/example_4_dummy_refs.cpp)
