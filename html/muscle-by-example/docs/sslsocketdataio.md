# muscle::SSLSocketDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SSLSocketDataIO.html)

```#include "dataio/SSLSocketDataIO.h"```

[SSLSocketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SSLSocketDataIO.html) works similarly to a [TCPSocketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1TCPSocketDataIO.html), except it runs encrypted over SSL instead of directly over TCP.

* In most cases to use [SSLSocketDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SSLSocketDataIO.html) correctly you'll also need to wrap your session's gateway in a [SSLSocketAdapterGateway](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SSLSocketAdapterGateway.html) object -- otherwise OpenSSL's internal state machine will get confused and not work correctly (OpenSSL is a bit of a mess architecturally :( )
* Depending on what you're doing, you'll probably want to call [SetPublicKeyCertificate()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SSLSocketDataIO.html#a359c1ca920cbb3aaf10438ed59cb617f) (if you're a client), or [SetPrivateKey()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1SSLSocketDataIO.html#a468fe88ed8a0e3735b8cf1ed29336cd8) (if you're a server) to handle authentication issues.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/dataio` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)
