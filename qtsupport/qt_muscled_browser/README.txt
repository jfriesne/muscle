This directory contains a Qt client program that lets you browse the contents of a muscled server
interactively in the GUI.  The client uses live subscriptions to generate its display, so the
display will always show the current contents of the server.

To compile under MacOS/X of Linux:

   qmake; make

To compile under Windows:

   qmake; nmake

Then run the application from its icon, or by executing it from Terminal with no arguments.

-Jeremy

ps Thanks to Jean-François Mullet for the idea for this program, and the original implementation of it.
