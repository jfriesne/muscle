# muscle::Hashtable class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Hashtable.html)

```#include "util/Hashtable.h"```

* Templated container class `Hashtable<Key,Value>`:  A hash table class that maintains a user-defined iteration ordering.
* Similar to: `std::map<K,V>`, and `std::unordered_map<K,V>` (combined!), `java.util.Hashtable`, `QHash`, etc.
* O(1) `Get()`/`ContainsKey`()/`Put()`/`Remove()`
* O(N*log(N)) `SortByKey()`
* O(N*log(N)) `SortByValue()`
* O(1) `RemoveFirst()`/`RemoveLast()`
* O(1) `SwapContents()`
* O(1) `MoveToFront()`/`MoveToBack()`/`MoveBefore()`/`MoveBehind()`
* Iteration of key/value pairs is done via `HashtableIterator` class
* Addresses of key and value objects never change (except when the `Hashtable`'s internal array is reallocated)
* No heap allocations/deletions during use (except when the `Hashtable`'s internal array is reallocated)
* It's okay to modify or delete the `Hashtable` during an iteration -- any active `HashtableIterator`(s) will handle the modification gracefully.
* Unlike many other hash table implementations, all `Hashtable` operations remain fully efficient even when the table is at 100% load-factor.

This class is typically used when quick key->value lookups are desired, or when the software wants to maintain a set of data that automatically enforces the uniqueness of keys in a key->value set.  Since it maintains the iteration-ordering of its contents, it can also be used as a keyed FIFO or LRU cache.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/hashtable` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [hashtable/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/hashtable/example_1_basic_usage.cpp)
* [hashtable/example_2_sorting.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/hashtable/example_2_sorting.cpp)
* [hashtable/example_3_iterating.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/hashtable/example_3_iterating.cpp)
* [hashtable/example_4_idioms.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/hashtable/example_4_idioms.cpp)
* [hashtable/example_5_ordered_tables.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/hashtable/example_5_ordered_tables.cpp)
* [hashtable/example_6_key_types.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/hashtable/example_6_key_types.cpp)
