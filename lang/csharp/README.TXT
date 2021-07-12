
This folder contains a C# implementation of the MUSCLE client API.

The code in this folder was converted from MUSCLE Java codebase 
by Wilson Yeung (wilson@whack.org), and donated back to the MUSCLE 
distribution on 6/26/2004.  It was last updated on 1/2/2005.

Please direct all C# related questions to Wilson, as I don't know
anything about that language.  :^)

-Jeremy

-----------------------------------------------------------------------

Hi Jeremy,

Please find attached a zip archive of an initial version of a C# muscle 
client.

It's far from perfect, but it does seem to work.  I've tested it using 
Mono on Mac OS X and Visual Studio 7.1, although I confess that most of 
the testing was on Mac OS X.

Mostly a line for line port of the Java muscle client library, although 
I changed the "MessageTransceiver" class to simply "Client" -- may 
change it back to MessageTransceiver later.

Things still to be done:

1. The Client class found in Client.cs uses a blocking select loop 
(although all sends and receives with the socket are non-blocking).  
This causes send latency as the select needs to timeout before it gets 
to waiting messages in the send queue.  I should fix this by using the 
.NET asynchronous send model (ie. Socket.BeginSend, 
Socket.BeginReceive, etc), but I've been so swamped at work I haven't 
gotten around to it yet.

2. Better error checking.

3. More documentation.

4. More classes in the test directory.  More examples in general.  An 
implementation of portablereflectclient.

Hope this email finds you well.

Wilson

