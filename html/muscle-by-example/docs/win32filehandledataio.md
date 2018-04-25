# muscle::Win32FileHandleDataIO class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Win32FileHandleDataIO.html)

```#include "winsupport/Win32FileHandleDataIO.h"```

[Win32FileHandleDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Win32FileHandleDataIO.html) is used to read/write data from a Windows [HANDLE](https://stackoverflow.com/questions/902967/what-is-a-windows-handle?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa) (an opaque type, as returned by [CreateFile()](https://msdn.microsoft.com/en-us/library/windows/desktop/aa363858(v=vs.85).aspx), etc).

* This class can only be used under Windows
* Typically used as a way to interface OS-neutral MUSCLE code to a Win32-specific I/O API.
* The [Win32FileHandleDataIO](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Win32FileHandleDataIO.html) object will assume ownership of the `HANDLE` handle you pass in to it, so don't need to (and shouldn't) call `CloseHandle()` on it yourself.
