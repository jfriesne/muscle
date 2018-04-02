# muscle::Queue class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Queue.html)

```#include "util/Queue.h"```

* Templated container class `Queue<T>`:  Double-ended, variable-sized vector/queue/ring-buffer, implemented using an internal array and modulo-arithmetic
* Similar to: `std::dequeue<T>`, `std::vector<T>`
* O(1) item-get/item-set (via `[] operator` or `GetItemAt()`/`ReplaceItemAt()`)
* O(1) `AddTail(const ItemType &)` (analogous to `push_back()`)
* O(1) `RemoveTail(const ItemType &)` (analogous to `pop_back()`)
* O(1) `AddFront(const ItemType &)` (analogous to `push_front()`)
* O(1) `RemoveFront(const ItemType &)` (analogous to `pop_front()`)
* O(N*log(N)) `Sort()`
* O(N) `RemoveItemAt()`/`RemoveItemsAt()`/`InsertItemAt()`/`InsertItemsAt()`
* O(1) `SwapContents()`

This class is typically used when an expandable-array is desired (`std::vector<T>` style), or an efficient FIFO-queue/ring-buffer is desired (`std::dequeue<T>` style), 

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/queue` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant test programs:

* [queue/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/queue/example_1_basic_usage.cpp)
* [queue/example_2_sorting.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/queue/example_2_sorting.cpp)
* [queue/example_3_idioms.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/queue/example_3_idioms.cpp)
