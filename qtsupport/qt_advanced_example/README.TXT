This program demonstrates a trick with the QMessageTransceiverThread class.

Note that this example is rather complex, so if you are looking for a simple example
of using Qt, check out the qt_example or qt_muscled or qt_muscled_browser folders instead.

Since the MessageTransceiverThread class contains a full-fledged ReflectServer object, that
it uses for its event loop, it can technically do anything that a muscled server can do.

That means that it is possible to have your Qt+MUSCLE GUI program run what is for all
intents and purposes a muscled server in a thread, and communicate with the muscled
thread via MessageRef objects, track which sessions it has connected, start your own
sessions on the muscled server, and so on.

This example program demonstrates how to do that.  It also demonstrates another trick:
how to create a session object that has its own internal thread inside it, which can be
used (with care) to do asynchronous processing or a third-party event loop that doesn't
block the rest of the muscled thread while it runs.

To build and run this Qt example:

1) Make sure you have the Qt development packages installed

2) In this folder, build the qt_advanced_example ("qmake; make")

3) Run the qt_advanced_example program (Under Linux, "./qt_advanced_example"; under MacOS/X, "./qt_advanced_example.app/Contents/MacOS/qt_advanced_example", under Windows, "Debug\qt_advanced_example.exe"), or click on its icon.

4) Follow the instructions shown in the window.

Some notes about the files in this folder:

qt_advanced_example.{cpp,h}
   These files contain the Qt GUI code used to implement a minimal user interface.
   Since the GUI isn't really the point of the example, the user interface is fairly rudimentary.

AdvancedQMessageTransceiverThread.{cpp,h}
   These files implement some slight customization of the QMessageTransceiverThread class and its
   related session classes, so that you can use a AdvancedQMessageTransceiverThread object as a
   local (in-GUI-program) MUSCLE server itself, rather than just as a mechanism for connecting
   to a MUSCLE server that is running somewhere else.

ThreadedInternalSession.{cpp,h}
   These files implement a session class that spawns its own internal thread.  The internal
   thread doesn't do much -- it just responds to Messages sent to it by the MUSCLE thread,
   and also increments a counter once a second.  When it increments its counter, it sends
   a PR_COMMAND_SETDATA Message back to the MUSCLE thread so that anyone subscribed to the
   path "/*/*/thread_status" (such as the QtGUI code, or any other MUSCLE client you care
   to connect to the server) can see the updates occur.  This is just stand-in behavior; a
   real program would do something more useful in the internal thread.

If you want to see where the various C++ objects are located within the various threads
in this program and how they communicate, be sure to check out the diagram
qt_advanced_example_code_layout_diagram.png that is in this folder.

-Jeremy
9/17/2011
