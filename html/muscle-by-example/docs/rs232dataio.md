# muscle::RS232DataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1RS232DataIO.html)

```#include "dataio/RS232DataIO.h"```

* `RS232DataIO` is used to read/write data from a serial port.
* The port to use and baud rate can be specified.
* Works with `select()`/`SocketMultiplexer` (even under Windows!)
* `RS232DataIO::GetAvailableSerialPortNames()` provides a list of available serial ports.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
