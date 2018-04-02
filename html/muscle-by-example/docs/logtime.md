# Logging API [(API)](https://public.msli.com/lcs/muscle/html/group__systemlog.html)

```#include "syslog/SysLog.h"```

* MUSCLE's way of emitting timestamped log text for debugging and status purposes
* Log output is tagged with one of the following severity levels
    * MUSCLE_LOG_TRACE   - for fine-grained tracing of program execution
    * MUSCLE_LOG_DEBUG   - for use during debugging only
    * MUSCLE_LOG_INFO    - normal severity level, for informational messages
    * MUSCLE_LOG_WARNING - warning about a potential problem
    * MUSCLE_LOG_ERROR   - report of an error (but program execution can continue)
    * MUSCLE_LOG_CRITICALERROR - report of a critical error (program execution may be in trouble)
* Log() outputs just the specified text, e.g. `Hello.`
* LogTime() outputs the specified text with a prefix, e.g. `[I 03/20 11:45:56] Hello.`
* Strings passed to Log() and LogTime() should include newline chars where appropriate to indicate the end of a log-line (e.g. `LogTime(MUSCLE_LOG_INFO, "Hi!\n");`)
* printf()-style variable interpolation is supported (e.g. `LogTime(MUSCLE_LOG_INFO, "3+2=%i\n", 5);`)
* Log text can be sent to stdout and/or to a log file
* Supports automatic creation, rotation, and deletion of log files, if desired.
* You can also register a `LogCallback` or `LogLineCallback` object to execute a user-supplied callback routine whenever text is logged.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/logtime` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [logtime/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/logtime/example_1_basic_usage.cpp)
* [logtime/example_2_log_files.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/logtime/example_2_log_files.cpp)
* [logtime/example_3_advanced_log_files.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/logtime/example_3_advanced_log_files.cpp)
* [logtime/example_4_log_callbacks.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/logtime/example_4_log_callbacks.cpp)
