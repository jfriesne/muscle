# muscle::DataNode class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html)

```#include "reflector/DataNode.h"```

[DataNode](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) is used by the [StorageReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSession.html) class to represent one node in the MUSCLE server-side node-tree-database.

* Each [DataNode](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) contains one [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) (stored via [MessageRef](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html)) and a [Hashtable](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Hashtable.html) of zero of more child nodes.
* [DataNode](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) objects keep track of which [StorageReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSession.html)s are currently subscribed to them, and call a function on those [StorageReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSession.html) objects whenever the state of the [DataNode](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) changes.
* A [DataNode](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) object can optionally keep a child-node-index that tracks the ordering of its children and informs subscribed clients when that ordering changes.  This functionality requires some extra overhead, so it is not enabled by default.  (It gets activated the first time a [DataNode](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) has its [InsertOrderedChild()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html#ade68425677a6241046ad824fce0b37e3) method called)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/reflector` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
