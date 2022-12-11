# muscle::DataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html)

```#include "dataio/DataIO.h"```

[DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) is an interface to any object that can read or write raw I/O data.

* Similar to:  [QIODevice](http://doc.qt.io/qt-5/qiodevice.html), [BDataIO](https://www.haiku-os.org/docs/api/classBDataIO.html)
* Used to unify networking, file, child-process, RS-232, and various other I/O APIs behind a single generic interface
* A program written to use a [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) can be easily modified to talk to any kind of I/O hardware (see `muscle/test/hexterm.cpp` for a good example)
* Both blocking and non-blocking I/O are supported.
* [virtual io_status_t Read(void * buf, uint32 bufSize)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#af093606c8aab0b42632e89a372a0d4e8) is implemented by [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) subclasses to read data from the hardware into a buffer
* [virtual io_status_t Write(const void * buf, uint32 bufSize)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#ada04bde999b32675319dab05e797588c) is implemented by [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) subclasses to write data from a buffer to the hardware
* [GetReadSelectSocket()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#a79f783e44a3813399b51cfcf042f0f77) returns a reference to a socket that `select()` (or [SocketMultiplexer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SocketMultiplexer.html)) can use to tell when the I/O is ready-for-read.
* [GetWriteSelectSocket()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#aadce442d1fe19e956b595e2f828b153a) returns a reference to a socket that `select()` (or [SocketMultiplexer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SocketMultiplexer.html)) can use to tell when the I/O is ready-for-write.  (Often this is the same socket returned by [GetReadSelectSocket()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#a79f783e44a3813399b51cfcf042f0f77), but not always!)
* Basic I/O devices (that support only [Read()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#af093606c8aab0b42632e89a372a0d4e8) and [Write()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html#ada04bde999b32675319dab05e797588c) operations) should subclass [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) directly.
* File-like devices with `seek()` capability should subclass [SeekableDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SeekableDataIO.html) instead.
* Devices with UDP-style per-packet-addressing abilities should subclass [PacketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1PacketDataIO.html) instead.
* Classes that want to act as a "facade" in front of any existing DataIO object (to unintrusively modify its behavior) should subclass [ProxyDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ProxyDataIO.html) instead.
* [DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html) class methods can be called directly (as shown in the dataio examples folder), but their primary use-case is to be installed (via [DataIORef](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataIO.html)) into an [AbstractMessageIOGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1AbstractMessageIOGateway.html) object as part of the High-Level Messaging API.
* Some direct subclasses of DataIO are:  [TCPSocketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1TCPSocketDataIO.html), [StdinDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StdinDataIO.html), [RS232DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RS232DataIO.html), [ChildProcessDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1ChildProcessDataIO.html), [NullDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1NullDataIO.html).

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:
    
* [dataio/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_1_basic_usage.cpp)
* [dataio/example_2_tcp_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_2_tcp_server.cpp)
* [dataio/example_3_seekable_dataio.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_3_seekable_dataio.cpp)
* [dataio/example_4_idioms.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_4_idioms.cpp)
* [dataio/example_5_packet_dataio.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_5_packet_dataio.cpp)
* [dataio/example_6_child_process.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/dataio/example_6_child_process.cpp)
