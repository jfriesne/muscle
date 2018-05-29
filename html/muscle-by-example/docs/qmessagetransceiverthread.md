# muscle::QMessageTransceiverThread class [(API)](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QMessageTransceiverThread.html)

```#include "qtsupport/QMessageTransceiverThread.h"```

[QMessageTransceiverThread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QMessageTransceiverThread.html) is a Qt-specific subclass of [MessageTransceiverThread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html).

It adds some glue code, so that incoming events emit Qt signals (so no need to call [MessageTransceiverThread::GetNextEventFromInternalThread()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1MessageTransceiverThread.html#a8aaee10a3afbb93be853d780814e857f) manually, instead just `connect()` the appropriate signals from the [QMessageTransceiverThread](https://public.msli.com/lcs/muscle/html/classmuscle_1_1QMessageTransceiverThread.html) object to the appropriate slots in your Qt/GUI object(s))

Try compiling and running the mini-example-program in `muscle/qtsupport/qt_example` (enter `qmake; make` to compile qt_example.app, and then run qt_example.app from Terminal or via its icon.  See the README.txt in that folder for more details)

Quick links to source code of relevant test programs:

* [muscle/qtsupport/qt_example/qt_example.cpp](https://public.msli.com/lcs/muscle/muscle/qtsupport/qt_example/qt_example.cpp)
* [muscle/qtsupport/qt_example/qt_example.h](https://public.msli.com/lcs/muscle/muscle/qtsupport/qt_example/qt_example.h)
* [muscle/qtsupport/qt_muscled_browser/Browser.cpp](https://public.msli.com/lcs/muscle/muscle/qtsupport/qt_muscled_browser/Browser.cpp)
* [muscle/qtsupport/qt_muscled_browser/Browser.h](https://public.msli.com/lcs/muscle/muscle/qtsupport/qt_muscled_browser/Browser.h)
