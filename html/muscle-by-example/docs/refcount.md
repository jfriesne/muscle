# muscle::RefCountable class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RefCountable.html)

```#include "util/RefCount.h"```

* Reference counting for heap-allocated objects (makes memory leaks and use-after-free errors impossible)
* Similar to: `std::shared_ptr<T>`, `boost::intrusive_ptr<T>`, `QSharedPointer`
* Object to be reference-counted must be a subclass of `RefCountable`
* The ref-counted object will always be deleted by the destructor of the last `Ref` that points to it -- explicitly calling `delete` should NEVER be necessary!
* Often used in conjunction with the `ObjectPool` class to recycle used objects (to minimize heap allocations/deallocations)
* `RefCountable` uses an `AtomicCounter` internally for its ref-count, so it's safe to pass `Ref` and `ConstRef` objects around to different threads.
* Idiom:  For any `RefCountable` class `FooBar`, typedefs `FooBarRef` and `ConstFooBarRef` are defined (via the `DECLARE_REFTYPES()` macro):

```
    class FooBar : public RefCountable { [...] };
    DECLARE_REFTYPES(FooBar);  // macro to define FooBarRef and ConstFooBarRef types

    FooBarRef oneRef(new FooBar);  // same as Ref<FooBar> oneRef(new FooBar);
    ConstFooBarRef twoRef(new FooBar);  // same as ConstRef<FooBar> twoRef(new FooBar);

    twoRef()->Hello();  // () operator gives pointer to RefCountable object
```

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/refcount` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [refcount/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/refcount/example_1_basic_usage.cpp)
* [refcount/example_2_with_object_pool.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/refcount/example_2_with_object_pool.cpp)
* [refcount/example_3_conversions.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/refcount/example_3_conversions.cpp)
* [refcount/example_4_dummy_refs.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/refcount/example_4_dummy_refs.cpp)
