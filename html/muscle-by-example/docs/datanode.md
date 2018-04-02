# muscle::DataNode class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html)

```#include "reflector/DataNode.h"```

* `DataNode` is used by the `StorageReflectSession` class to represent one node in the MUSCLE server-side node-tree-database.
* Each `DataNode` contains one `Message` (stored via `MessageRef`) and a `Hashtable` of zero of more child nodes.
* `DataNode` objects keep track of which `StorageReflectSession`s are currently subscribed to them, and call a function on those `StorageReflectSession` objects whenever the state of the `DataNode` changes.
* A `DataNode` object can optionally keep a child-node-index that tracks the ordering of its children and informs subscribed clients when that ordering changes.  This functionality requires some extra overhead, so it is not enabled by default.  (It gets activated the first time a `DataNode` has its `InsertOrderedChild()` method called)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/reflector` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
