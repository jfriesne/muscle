# muscle::Queue class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Queue.html)

```#include "util/Queue.h"```

Templated container class `Queue<T>` is a bidirectional variable-sized vector/queue/ring-buffer, implemented using an internal array and modulo-based index-arithmetic

* Similar to: [std::deque&lt;T&gt;](http://en.cppreference.com/w/cpp/container/deque), [std::vector&lt;T&gt;](http://en.cppreference.com/w/cpp/container/vector), [std::queue&lt;T&gt;](http://en.cppreference.com/w/cpp/container/queue), [std::stack&lt;T&gt;](https://en.cppreference.com/w/cpp/container/stack), [QVector](http://doc.qt.io/qt-5/qvector.html)
* O(1) item-get/item-set (via [[] operator](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Queue.html#a5a9b58841adef9e3d8978d1490df3ab6) or [GetItemAt()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Queue.html#a3a150406a8258bfc72c50d1169348ebf)/[ReplaceItemAt()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Queue.html#a6cf11cdfbe2a2d22da790b935f47382c))
* O(1) [AddTail(const ItemType &)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Queue.html#acf18e2f21b3bb109f17de4c3b98c6bac) (analogous to `push_back()`)
* O(1) [RemoveTail(const ItemType &)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Queue.html#aad2ed7d57daad0f25422d7ce68611d77) (analogous to `pop_back()`)
* O(1) [AddHead(const ItemType &)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Queue.html#a45fd77f633ab4add7448a1761cf3d9f6) (analogous to `push_front()`)
* O(1) [RemoveHead(const ItemType &)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Queue.html#af7aea85455c12e3dd3ff188162163fff) (analogous to `pop_front()`)
* O(N*log(N)) [Sort()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Queue.html#adcebfdb95b9fd861b5eae118eab9f0c1)
* O(N) [RemoveItemAt()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Queue.html#ad8c2c4cd3d230239e37fadc928523d3a)/[InsertItemAt()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Queue.html#a652a3c4c32f3efa0dd2e2529d32b6c32)/[InsertItemsAt()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Queue.html#a1f629464d7989dd9247b518d9e970e54)
* O(1) [SwapContents()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Queue.html#a64feb05ea05f3ca69df0a5fc91d4c68d)

This class is typically used when an expandable-array is desired (`std::vector<T>`-style), or an efficient FIFO-queue/ring-buffer is desired (`std::deque<T>`-style).

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/queue` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [queue/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/queue/example_1_basic_usage.cpp)
* [queue/example_2_sorting.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/queue/example_2_sorting.cpp)
* [queue/example_3_idioms.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/queue/example_3_idioms.cpp)
