
This folder contains an alpha C implementation of the MUSCLE Message
and MessageGateway APIs.

If you want to avoid C++ in your program (usually because you want
to minimize executable size, or because a C++ compiler is not available)
you can use these APIs to implement simple MUSCLE Message functionality
instead.

These files do not depend on any other files outside of this folder,
except for support/MuscleSupport.h.

The MiniMessage.{c,h} files contain the MiniMessage API -- the
equivalent to the Message class in C++.  There are some significant
differences between the two implementations, however:

1) MMessages cannot be allocated on the stack -- you must allocate
   them on the heap, by calling MMAllocMessage().  When you are done
   using the MMessage, you must manually free it by calling MMFreeMessage(),
   or you will leak memory.

2) MMessages do not implement reference counting; they use simple
   object-ownership semantics instead.  This means that when you
   e.g. add a child MMessage to another MMessage, the child MMessage
   becomes the property of the parent MMessage and will be freed
   when the parent MMessage is freed, etc.

3) Unlike the Message class, where fields in the Message are dynamically
   extensible, the MMessage API requires you to specify the number of
   items in a field in advance.  For example, to add three int32's to
   a C++ Message, you could do this:

     Message msg;
     msg.AddInt32("fieldname", 1);
     msg.AddInt32("fieldname", 2);
     msg.AddInt32("fieldname", 3);
     [...]


   The same code using the MMessage API would look like this:

     MMessage * msg = MMAllocMessage(0);
     int32 * field = MMPutInt32Field(msg, false, "fieldname", 3);
     field[0] = 1;
     field[1] = 2;
     field[2] = 3;
     [...]
     MMFreeMessage(msg);
     
   It's a bit less flexible, but it makes the implementation much
   simpler and more efficient.  See the test file tests/testmini.cpp
   for example usage of the MMessage API.

4) The MMessageGateway API doesn't use DataIO objects to interface
   with the I/O API -- instead, it uses function callbacks.  There
   is one function callback for reading data, and another for
   writing data, and your pass in these function pointers as part 
   of your calls to MGDoInput() and MGDoOutput().  See the test
   file tests/minireflectclient.c for an example of how this
   is used.


