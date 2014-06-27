As of v1.60, some Java classes have been included with MUSCLE to make it
easier to create Java-based MUSCLE clients.  These classes require Java 1.4.0
or higher to compile.  The MUSCLE server code is still C++ only, though.  

The included Java classes are:

package com.meyer.muscle.message:   Java equivalent of Message class
  FieldNotFoundException        - thrown if you try to access a field that isn't in a message
  FieldTypeMismatchException    - thrown if you try to access a field by the wrong type
  Message                       - used similarly to the C++ Message, or Be's BMessage class
  MessageException              - base class for all exceptions thrown by Message class

package com.meyer.muscle.queue
  Queue                         - A double-ended queue, like the C++ Queue or STL dequeue classes

package com.meyer.muscle.support:   Miscellaneous stuff
  LEDataInputStream             - Adaptor to make an input stream little-endian (courtesy Canadian Moose Products)
  LEDataOutputStream            - Adaptor to make an output stream little-endian (courtesy Canadian Moose Products)
  Flattenable                   - Interface for flattenable objects (similar to the Flattenable C++ class)
  Point                         - Similar to BPoint (java.awt.Point wasn't used because we need floats, not ints)
  Rect                          - Similar to BRect (java.awt.Rectangle wasn't used for the same reason)
  TypeConstants                 - Defines the B_*_TYPE constants
  UnflattenFormatException      - Exception that is thrown when an unflatten() call fails

package com.meyer.muscle.iogateway: Adaptor classes for converting Messages to and from byte streams
  AbstractMessageIOGateway      - Abstract interface for a message IO gateway
  MessageIOGateway              - Converts Messages to standard MUSCLE byte streams
  MessageIOGatewayFactory       - A factory object that knows how to create various MessageIOGateway objects
  NativeZLibMessageIOGateway    - A gateway that uses Java's built-in zlib library to send/receive compressed Messages
  JZLibMessageIOGateway         - Same as NAtiveZLibMessageIOGateway, but uses the JCraft ZLib library instead

package com.meyer.muscle.thread:    Some nice threading/message queue support
  MessageListener               - Interface for a class that wants to receive asynchronous messageReceived() callbacks
  MessageQueue                  - A class that processes a stream of message objects and gives them to MessageListeners
  ThreadPool                    - Holds a set of Threads and reuses them

package com.meyer.muscle.client:    Some higher-level convenience classes for easy client-side networking
  MessageTransceiver            - A threaded TCP message transceiver (somewhat like the C++ MessageTransceiverThread class)
  StorageReflectConstants       - Java declarations of the constants used in the StorageReflectSession protocol
  DatagramPacketTransceiver     - A special class for packetizing/depacketizing MUSCLE Messages over UDP packets

package com.meyer.muscle.test:      Code testing/verification programs
  TestClient.java               - A command line test client, similar to the C++ testreflectclient program.
  UDPClient.java                - A command line test client, to test the DatagramPacketTransceiver class.


Quick Java Client HOWTO
-----------------------

Writing a Java MUSCLE client is easy.  Here is what you need to do:

1. Integrate all the com.meyer.muscle classes in the muscle/java subdirectory into your Java project.

2. Have a class in your application that implements the com.meyer.muscle.thread.MessageListener interface.
   Then, when your program wishes to connect to a MUSCLE server, it should use a MessageTransceiver, like this:

      MessageTransceiver mt = new MessageTransceiver(new MessageQueue(this));  // assumes (this) implements MessageListener
      mt.connect("beshare.bentonrea.com", 2960, onSuccess, onFailure);  // or wherever

   (onSuccess) and (onFailure) can be any objects you like; they will be passed back to you in your
   messageReceived() method when the connection is completed, or when it (fails/is broken), respectively.

3. To send a Message to the server, just create it and send it out using the MessageTransceiver, for example:

      Message msg = new Message(PR_COMMAND_FOOBAR);    // construct the message...
      msg.setString("SomeStringField", "MyValue");
      msg.setInt("AnIntValue", 35);
      float someFloats[] = {1.0f, 2.5f, 4.0f};
      msg.setFloats("SomeFloatValues", someFloats);
      mt.sendOutgoingMessage(msg);                     // and queue it up to be transmitted asynchronously

   Note that the methods in Message are used differently than in the C++ class;  you must set
   each field's contents with a single method call, instead of adding values one at a time with repeated calls.
   Also, the methods are named after Java data types instead of BeOS types:

      Java             C++/BeOS
      ===============================
      boolean          bool
      byte             int8
      short            int16
      int              int32
      long             int64
      float            float
      double           double
      java.lang.String String 
      
4. When a Message is received from the server, your MessageListener's messageReceived() method is called
   so you can process it.  Note that messageReceived() is called from a background Thread, so you probably want 
   to declare it synchronized to avoid potential race conditions:

   class MyClass implements MessageListener {
      public synchronized void messageReceived(Object msg, int numLeftInQueue) throws Exception
      {
         if (msg == onSuccess)  // as referenced in the mt.connect() call
         {
            System.out.println("TCP Connection succeeded!");
         }
         else if (msg == onFailure)  // as referenced in the mt.connect() call
         {
            System.out.println("TCP Connection failed, or was disconnected");
         }
         else if (msg instanceof Message)
         {
            Message p = (Message) msg;
            System.out.println("Received a Message from the server: ");
            p.printToStream(System.out);
         }
      }

      [...]
   }
    

5. When you wish to close the TCP connection, call disconnect() on your MessageTransceiver object.
   Note that you may still receive calls to your messageReceived() method even after the disconnect()
   call returns, due to the asynchronous nature of the message queueing.

And that's it!  All Message semantics are the same as they are in C++ land; see the
"Beginner's Guide to MUSCLE" HTML document, or the comments at the end of StorageReflectConstants.java
for details.  There is also a JavaChat client, with source code, on BeBits.com, that allows you
to chat with BeShare users from a Java applet.  You can use that as example source too.
