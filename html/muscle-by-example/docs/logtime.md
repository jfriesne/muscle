# Logging API [(API)](https://public.msli.com/lcs/muscle/html/group__systemlog.html)

```#include "syslog/SysLog.h"```

MUSCLE's logging mechanism, for debugging, troubleshooting, and status/monitoring purposes.

* [LogPlain()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga967e05bef1f65995466ee17ff37ae15b) outputs just the specified text, e.g. `Hello.`
* [LogTime()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga347d199c3019a4ad8d474befded2b25f) outputs the specified text with a prefix, e.g. `[I 03/20 11:45:56] Hello.`
* Each line of log-output is tagged with one of the following [severity levels](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ggaabfcbcb5ac86a1edac4035264bc7d2b8ab59c09ea8b69899ae8812ab2ac1e6f8e)
    * `MUSCLE_LOG_TRACE`   - for fine-grained tracing of program execution
    * `MUSCLE_LOG_DEBUG`   - for use during debugging only
    * `MUSCLE_LOG_INFO`    - normal severity level, for informational messages
    * `MUSCLE_LOG_WARNING` - warning about a potential problem
    * `MUSCLE_LOG_ERROR`   - report of an error (but program execution can continue)
    * `MUSCLE_LOG_CRITICALERROR` - report of a critical error (program execution may be in trouble)
* Strings passed to [LogPlain()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga967e05bef1f65995466ee17ff37ae15b) and [LogTime()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga347d199c3019a4ad8d474befded2b25f) should include newline chars where appropriate to indicate the end of a log-line (e.g. `LogTime(MUSCLE_LOG_INFO, "Hi!\n");`)
* `printf()`-style variable interpolation is supported (e.g. `LogTime(MUSCLE_LOG_INFO, "3+2=%i\n", 5);`)
* Log-text can be sent to stdout and/or to a log file.
* [LogTime()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga7a1febce28c8d4c7937b2fc53e068d8a) and [LogPlain()](https://public.msli.com/lcs/muscle/html/group__systemlog.html#ga7fba38edf46b321ea42a15e6c69dbae9) are implemented as macros such that their arguments will only be evaluated if the log-text is not going to be filtered.  This makes it efficient to include debug/trace logging even in tight loops.
* Supports automatic creation, rotation, tgz-compression, and/or deletion of log files, if desired.
* You can also register a [LogCallback](https://public.msli.com/lcs/muscle/html/classmuscle_1_1LogCallback.html) or [LogLineCallback](https://public.msli.com/lcs/muscle/html/classmuscle_1_1LogLineCallback.html) object to execute a user-supplied callback routine whenever text is logged.

Try compiling and running the mini-example-programs in `muscle/html/muscle-by-example/examples/logtime` (enter `make` to compile example_*, and then run each from Terminal while looking at the corresponding .cpp file)

Quick links to source code of relevant MUSCLE-by-example programs:

* [logtime/example_1_basic_usage.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/logtime/example_1_basic_usage.cpp)
* [logtime/example_2_log_files.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/logtime/example_2_log_files.cpp)
* [logtime/example_3_advanced_log_files.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/logtime/example_3_advanced_log_files.cpp)
* [logtime/example_4_log_callbacks.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/logtime/example_4_log_callbacks.cpp)
