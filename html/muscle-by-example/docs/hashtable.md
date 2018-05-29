# muscle::Hashtable class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Hashtable.html)

```#include "util/Hashtable.h"```

Templated container class `Hashtable<Key,Value>` is a hash-based dictionary class that also maintains a consistent, user-specifiable iteration order.

* Similar to: [std::map&lt;K,V&gt;](http://en.cppreference.com/w/cpp/container/map), and [std::unordered_map&lt;K,V&gt;](http://en.cppreference.com/w/cpp/container/unordered_map) (combined!), [java.util.Hashtable](https://docs.oracle.com/javase/8/docs/api/java/util/Hashtable.html), [QHash](http://doc.qt.io/qt-5/qhash.html), etc.
* O(1) [Get()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableBase.html#a19543125698485a668ce2cc53c8c057d)/[ContainsKey()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableBase.html#a790ed182d252511a38574a741953ccbd)/[Put()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableMid.html#a59691a7665446505535cf02aadfdebf3)/[Remove()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableBase.html#aacb659ec030127644ef076f0f70d6f92)
* O(1) [RemoveFirst()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableBase.html#a2f3bac865a3ffbeaeb6f63117d9a76b4)/[RemoveLast()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableBase.html#a9236392a40ab749ece15edb5bf1f6d3d)
* O(N*log(N)) [SortByKey()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableBase.html#aed9bd2abb05b26914a83735bed57c3e3)
* O(N*log(N)) [SortByValue()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableBase.html#a9fc3e1a503a8889a0cacb6ed1ef7f9ec)
* O(1) [MoveToFront()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableBase.html#a4216f4cb9567bb38ab4e42f546b4e3a8)/[MoveToBack()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableBase.html#ade6408ab7e42d5dcd2b9924910c2d528)/[MoveToBefore()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableBase.html#a71e3c5d80457fd0e59fb5cc5e1eb72c2)/[MoveToBehind()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableBase.html#a3c2935ce21a8855d3128b79c52ece19d)
* O(1) [SwapContents()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Hashtable.html#a40d409c769d9ada96a45b3bfcca39481)
* Iteration of key/value pairs is done via [HashtableIterator](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableIterator.html) class
* Addresses of key and value objects never change (except when the [Hashtable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Hashtable.html)'s internal array is reallocated)
* No heap allocations/deletions during use (except when the [Hashtable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Hashtable.html)'s internal array is reallocated)
* If you know how many key/value pairs you need to store, you can (optionally) call [EnsureSize()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableMid.html#a7a96b0de767c2bc01e0d86686c54545d) up-front to avoid unnecessary array-reallocations while adding data.
* It's okay to modify or delete the [Hashtable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Hashtable.html) during an iteration -- any active [HashtableIterator](https://public.msli.com/lcs/muscle/html/classmuscle_1_1HashtableIterator.html)(s) will handle the modification gracefully.
* Unlike many other hash table implementations, all [Hashtable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Hashtable.html) operations remain fully efficient even when the table is at 100% load-factor -- so there's no need to reallocate the underlying array until it has become completely full.

This class is typically used when efficient key->value lookups are desired, or when the software wants to maintain a set of data that automatically enforces the uniqueness of keys in a key->value set.  Since it maintains the iteration-ordering of its contents, it can also be used as a keyed FIFO or LRU-cache.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/hashtable` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [hashtable/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/hashtable/example_1_basic_usage.cpp)
* [hashtable/example_2_sorting.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/hashtable/example_2_sorting.cpp)
* [hashtable/example_3_iterating.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/hashtable/example_3_iterating.cpp)
* [hashtable/example_4_idioms.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/hashtable/example_4_idioms.cpp)
* [hashtable/example_5_ordered_tables.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/hashtable/example_5_ordered_tables.cpp)
* [hashtable/example_6_key_types.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/hashtable/example_6_key_types.cpp)
