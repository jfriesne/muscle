This folder contains an extremely bare-bones C implementation of the 
MUSCLE Message and MessageGateway APIs.

If you want to avoid C++ in your program (usually because you want
to minimize executable size, or because a C++ compiler is not available)
you can use the MiniMessages APIs in the minimessage folder to 
implement simple MUSCLE Message functionality instead.

However, if even MiniMessage is too heavyweight, and you want an
implementation that avoids dynamic memory allocation altogether,
you can use this MicroMessage (aka "UMessage") implementation instead.

This implementation never bothers to flatten or unflatten Message
objects at all; instead it reads and writes flattened Message
data directly from a raw buffer of bytes.

That makes this implementation very efficient, but it does restrict
its functionality.  In particular when constructing a MicroMessage
buffer, data can only be appended.  Data already existing in the
MicroMessage buffer is considered read-only.  Also, you can only
add as much data as will fit into the byte-buffer you provided.
Attempts to add more data than that will fail.

These files do not depend on any other files outside of this folder,
except for support/MuscleSupport.h.

The MicroMessage.{c,h} files contain the MicroMessage API -- the
equivalent to the MiniMessage class in the minimessage folder, or 
the Message class in C++.  There are some significant
differences between the two implementations, however:

1) A UMessage object does not contain its own memory buffer --
   you must provide (and guarantee the lifetime of) your own
   external memory buffer that the UMessage object can
   read from and/or write to.

2) Unlike the Message class, where fields in the Message are dynamically
   extensible, the UMessage API requires you to specify the number of
   items in a field in advance.  For example, to add three int32's to
   a C++ Message, you could do this:

     Message msg;
     msg.AddInt32("fieldname", 1);
     msg.AddInt32("fieldname", 2);
     msg.AddInt32("fieldname", 3);
     [...]

   Equivalent code using the UMessage API would look like this:

     char buf[1024];  // we assume our UMessage data will fit in 1024 bytes!
     UMessage msg;
     UMInitialize(&msg, buf, sizeof(buf));

     const int32 vals[3] = {1, 2, 3};
     UMAddInt32s(&msg, "fieldname", vals, 3);
     
   It's a bit less flexible, but it makes the implementation much
   simpler and more efficient.  If you're just adding a single
   value, it's a bit easier:

     UMAddInt32(&msg, "fieldname", 42);

   See the test file tests/testmicro.cpp for more example usages of the UMessage API.
