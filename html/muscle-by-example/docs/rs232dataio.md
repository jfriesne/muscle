# muscle::RS232DataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RS232DataIO.html)

```#include "dataio/RS232DataIO.h"```

[RS232DataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RS232DataIO.html) is used to read/write data to/from a serial port.

* The port to use and baud rate can be specified.
* Works with `select()`/[SocketMultiplexer](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SocketMultiplexer.html) (even under Windows!)
* [RS232DataIO::GetAvailableSerialPortNames()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RS232DataIO.html#a33786b1519fd039e0d613ca1bdc50955) provides a list of available serial ports.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
