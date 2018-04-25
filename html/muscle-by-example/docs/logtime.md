# Logging API [(API)](https://public.msli.com/lcs/muscle/html/group__systemlog.html)

```#include "syslog/SysLog.h"```

MUSCLE's logging mechanism, for debugging, troubleshooting, and status/monitoring purposes.

* [Log()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga2e205a37885683d43d599490546077db) outputs just the specified text, e.g. `Hello.`
* [LogTime()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga6e1590149dd8ffb11790f6965369fb16) outputs the specified text with a prefix, e.g. `[I 03/20 11:45:56] Hello.`
* Each line of log-output is tagged with one of the following [severity levels](https://public.msli.com/lcs/muscle/html/group__systemlog.html#gga94798fdadfbf49a7c658ace669a1d310ab59c09ea8b69899ae8812ab2ac1e6f8e)
    * MUSCLE_LOG_TRACE   - for fine-grained tracing of program execution
    * MUSCLE_LOG_DEBUG   - for use during debugging only
    * MUSCLE_LOG_INFO    - normal severity level, for informational messages
    * MUSCLE_LOG_WARNING - warning about a potential problem
    * MUSCLE_LOG_ERROR   - report of an error (but program execution can continue)
    * MUSCLE_LOG_CRITICALERROR - report of a critical error (program execution may be in trouble)
* Strings passed to [Log()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga2e205a37885683d43d599490546077db) and [LogTime()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga6e1590149dd8ffb11790f6965369fb16) should include newline chars where appropriate to indicate the end of a log-line (e.g. `LogTime(MUSCLE_LOG_INFO, "Hi!\n");`)
* `printf()`-style variable interpolation is supported (e.g. `LogTime(MUSCLE_LOG_INFO, "3+2=%i\n", 5);`)
* Log-text can be sent to stdout and/or to a log file.
* Supports automatic creation, rotation, and deletion of log files, if desired.
* You can also register a [LogCallback](https://public.msli.com/lcs/muscle/html/classmuscle_1_1LogCallback.html) or [LogLineCallback](https://public.msli.com/lcs/muscle/html/classmuscle_1_1LogLineCallback.html) object to execute a user-supplied callback routine whenever text is logged.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/logtime` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [logtime/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/logtime/example_1_basic_usage.cpp)
* [logtime/example_2_log_files.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/logtime/example_2_log_files.cpp)
* [logtime/example_3_advanced_log_files.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/logtime/example_3_advanced_log_files.cpp)
* [logtime/example_4_log_callbacks.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/logtime/example_4_log_callbacks.cpp)
