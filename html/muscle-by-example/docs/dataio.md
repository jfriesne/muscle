# muscle::DataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html)

```#include "dataio/DataIO.h"```

* Abstract Interface for any object that can read or write I/O data
* Similar to:  `QDataIO`, `BDataIO`
* Used to unify file, networking, child-process, RS-232, and various other I/O APIs behind a single generic interface
* A program written to use a DataIO can be easily modified to talk to any kind of I/O hardware (see `muscle/test/hexterm.cpp` for a good example)
* `virtual int32 Read(void * buf, uint32 bufSize)` is implemented by `DataIO` subclasses to read data from the hardware into a buffer
* `virtual int32 Write(const void * buf, uint32 bufSize)` is implemented by `DataIO` subclasses to write data from a buffer to the hardware
* `GetReadSelectSocket()` returns a reference to a socket that `select()` (or `SocketMultiplexer`) can use to tell when the I/O is ready-for-read.
* `GetWriteSelectSocket()` returns a reference to a socket that `select()` (or `SocketMultiplexer`) can use to tell when the I/O is ready-for-write.  (Often this is the same socket returned by `GetReadSelectSocket()`, but not always!)
* Basic I/O devices (that support only `Read()` and `Write()` operations) should subclass `DataIO` directly.
* File-like devices with `seek()` capability should subclass `SeekableDataIO` instead.
* Devices with UDP-style per-packet-addressing abilities should subclass `PacketDataIO` instead.
* Classes that want to act as a "facade" in front of any existing DataIO object (to unintrusively modify its behavior) should subclass `ProxyDataIO` instead.
* `DataIO` class methods can be called directly (as shown in the dataio examples folder), but their primary use-case is to be installed (via `DataIORef`) into an `AbstractIOMessageGateway` object as part of the High-Level Messaging API.
* Some direct subclasses of DataIO are:  [TCPSocketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1TCPSocketDataIO.html), [StdinDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StdinDataIO.html), [RS232DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RS232DataIO.html), [ChildProcessDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html), [NullDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1NullDataIO.html).

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:
    
* [dataio/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_1_basic_usage.cpp)
* [dataio/example_2_tcp_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_2_tcp_server.cpp)
* [dataio/example_3_seekable_dataio.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_3_seekable_dataio.cpp)
* [dataio/example_4_idioms.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_4_idioms.cpp)
* [dataio/example_5_packet_dataio.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_5_packet_dataio.cpp)
* [dataio/example_6_child_process.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_6_child_process.cpp)
