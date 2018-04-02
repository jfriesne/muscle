# muscle::ProxyDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ProxyDataIO.html)

```#include "dataio/ProxyDataIO.h"```

* `ProxyDataIO` is an Abstract Interface that extends both the `SeekableDataIO` and `PacketDataIO` interfaces (this works because of virtual inheritance).
* The purpose of the `ProxyDataIO` class is to serve as a base class for any "decorator-style" `DataIO`-subclass that wants to take an arbitrary existing DataIO object as an argument (typically by holding it via a `DataIORef`) and dynamically modify its behavior by standing in between the DataIO object and the calling code.
* For a simple example, see the `XorProxyDataIO` class -- it is a decorator object that transparently adds a per-byte XOR transformation to the data going into (or coming out of) the DataIO object it holds.
* Another example use-case is the `AsyncDataIO` class, which executes the operations of its held DataIO object in a separate thread, so that they won't/can't ever slow down the calling thread (not even if the held `DataIO` is doing blocking I/O).
* A third example use-case is the `PacketizedProxyDataIO` class, which inserts framing-data into its child DataIO's data stream so that the data will be received on the receiving side in the same-sized chunks that it was sent on the sending-side (thus avoiding every TCP programmer's first trip to [StackOverflow](http://www.stackoverflow.com) to ask why his recv() calls don't have a 1:1 correspondence to his send() calls)

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
