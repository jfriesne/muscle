This folder contains some Python code which implements client-side
support for the MUSCLE protocol for the Python language/environment.
File in this folder include:

message.py - Python implementation of the MUSCLE Message class.
             This typed-data container class forms the heart of
             the MUSCLE protocol.

message_transceiver_thread.py - A networking utility class that provides
                                a separate thread that will send and receive
                                Message objects over a TCP connection for you.
                                Using this to connect to a MUSCLE server or
                                peer is much easier than trying to do it
                                yourself using select(), etc.

storage_reflect_constants.py - This file contains some constants that
                               are meaningful to the default MUSCLE server.
                               The constants' names and values are the same
                               as they are for the C++ implementation.

python_chat.py - A VERY simple Python-based BeShare chat client, to
                 demonstrate how to use the Python MUSCLE APIs.


All public methods in these files are commented, and most files 
have well-commented testing stubs at the bottom that demonstrate 
how to use the class.

