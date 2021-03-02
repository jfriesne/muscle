#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "message/Message.h"
#include "dataio/StdinDataIO.h"
#include "dataio/TCPSocketDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates using a PlainTextMessageIOGateway to send\n");
   printf("and receive plain text over a TCP connection.\n");
   printf("\n");
   printf("Note that in this example, we set the TCPSocketDataIO to be in\n");
   printf("non-blocking mode.  The benefit here is that the PlainTextMessageIOGateway \n");
   printf("will handle all of the byte-queueing and text-line-reassembly steps for us.  \n");
   printf("Those are the things that make non-blocking mode such a PITA otherwise.\n");
   printf("\n");
   printf("Note that in a real program you probably wouldn't want to do it \n");
   printf("this way; you'd use a ReflectServer and a session object instead, \n");
   printf("and you'd set the session object's gateway to use a \n");
   printf("PlainTextMessageIOGateway, and you wouldn't have to write all of \n");
   printf("this I/O-handling code yourself.\n");
   printf("\n");
   printf("This example is written to use the PlainTextMessageIOGateway API directly\n");
   printf("just to demonstrate how a PlainTextMessageIOGateway works.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   ConstSocketRef tcpSock;

   const uint16 tcpPort = 23456;  // arbitrary

   // First we need to set up the TCP connection.  We'll do that either
   // by accepting an incoming TCP connection or making an outgoing one,
   // depending on the argument the user supplied.

   const String arg1 = (argc>1) ? argv[1] : GetEmptyString();
   if (arg1.EqualsIgnoreCase("accept"))
   {
      ConstSocketRef acceptSock = CreateAcceptingSocket(tcpPort);
      if (acceptSock())
      {
         printf("Waiting for incoming TCP connection on port %i...\n", tcpPort);
         printf("\n");
         printf("Now would be a good time to run \"./example_4_text_to_tcp connect\" in another Terminal window ...\n");
         printf("Or (since it's just plain-text-over-TCP), running \"telnet localhost %i\" would work too!\n", tcpPort);

         tcpSock = Accept(acceptSock);
         if (tcpSock()) printf("Incoming TCP connection accepted!\n");
         else
         {
            printf("Accept() failed, aborting!\n");
            return 10;
         }
      }
      else 
      {
         printf("CreateAcceptingSocket() failed!  Perhaps another program is already using port %i?\n", tcpPort);
         return 10;
      }
   }
   else if (arg1.EqualsIgnoreCase("connect"))
   {
      tcpSock = Connect(localhostIP, tcpPort, NULL, "example_4_text_to_tcp", false);
      if (tcpSock() == NULL)
      {
         printf("Connect() failed, aborting!  (Perhaps you forgot to run \"./example_4_text_to_tcp accept\" in another Terminal first?)\n");
         return 10;
      }
   }
   else 
   {
      printf("Usage:  ./example_4_message_to_tcp [accept] [connect]\n");
      printf("Run an instance with the \"accept\" keyword first, then\n");
      printf("a second instance with the \"connect\" keyword in another\n");
      printf("Terminal window.  Then you can type into either window to\n");
      printf("send a Message to the other instance.\n");
      return 10;
   }

   printf("\n");

   // Now that we have our connected TCP socket, we'll give it to a TCPSocketDataIO
   // and give the TCPSocketDataIO to our PlainTextMessageIOGateway.

   TCPSocketDataIO tcpIO(tcpSock, false);  // false == set socket to non-blocking mode!

   PlainTextMessageIOGateway gateway;
   gateway.SetDataIO(DataIORef(&tcpIO, false));  // false == don't delete the pointer!

   // And we'll use a StdinDataIO to collect stdin input from the user
   // (We could use a PlainTextPlainTextMessageIOGateway in conjunction with it, but for simplicity I won't)
   StdinDataIO stdinIO(true);

   printf("Main event loop starting -- you can type a sentence into stdin to send a Message to the other session.\n");
   printf("\n");

   // Our program's event-loop
   SocketMultiplexer sm;
   bool keepGoing = true;
   while(keepGoing)
   {
      // Tell the SocketMultiplexer what file descriptors to watch
      sm.RegisterSocketForReadReady(stdinIO.GetReadSelectSocket().GetFileDescriptor());
      sm.RegisterSocketForReadReady(gateway.GetDataIO()()->GetReadSelectSocket().GetFileDescriptor());
      if (gateway.HasBytesToOutput())
      {
         // We only care about the TCP socket's ready-for-write status if our gateway
         // actually has some data queued up to send.  Otherwise there is no point waking up
         // just because the TCP socket's output-buffer isn't full...
         sm.RegisterSocketForWriteReady(gateway.GetDataIO()()->GetWriteSelectSocket().GetFileDescriptor());
      }
    
      // Wait for something to happen (on either the TCP side or the stdin side)...
      sm.WaitForEvents();

      // Are there bytes ready-for-read on stdin?
      if (sm.IsSocketReadyForRead(stdinIO.GetReadSelectSocket().GetFileDescriptor()))
      {
         // The user typed something in to stdin!  Let's find out what he says
         char buf[1024];
         if (fgets(buf, sizeof(buf), stdin) == NULL)   // the quick-and-dirty way
         {
            printf("stdin was closed!  Exiting!\n");
            break;
         }

         String userText = buf;
         userText = userText.Trim();  // get rid of any newline
         if (userText.HasChars())
         {
            printf("You typed:  [%s]\n", userText());

            // Now let's create a Message containing the user's text and send it across the TCP
            MessageRef userMsg = GetMessageFromPool(PR_COMMAND_TEXT_STRINGS);
            userMsg()->AddString(PR_NAME_TEXT_LINE, userText);   // what the user typed in
    
            // And queue it up in the gateway for transmission ASAP
            printf("Your outgoing Message has been queued for transmission ASAP!\n");
            printf("Your outgoing Message is:\n");
            userMsg()->PrintToStream();

            gateway.AddOutgoingMessage(userMsg);
         }
      }

      // Are there bytes ready-for-read on the TCP socket?
      if (sm.IsSocketReadyForRead(gateway.GetDataIO()()->GetReadSelectSocket().GetFileDescriptor()))
      {
         // There are some!  Let's have the gateway read them in.  If the gateway
         // has enough to assemble a complete Message out of them, it will call 
         /// MessageReceivedFromGateway(msg) on the AbstractGatewayMessageReceiver 
         // object we pass in as an argument to gateway.DoInput().  In this case 
         // we'll just use a QueueGatewayMessageReceiver, whose 
         // MessageReceivedFromGateway() method simply adds the Message to a 
         // Queue for us to examing afterwards.

         QueueGatewayMessageReceiver qReceiver;
         while(true)
         {
            const int numBytesRead = gateway.DoInput(qReceiver);
            if (numBytesRead < 0)
            {
               printf("TCP connection closed!  Will quit ASAP.\n");
               keepGoing = false;
               break;
            }
            else if (numBytesRead == 0) break;  // no more bytes to read for now!
         }

         // Now that we're done reading (for now), print out any Messages we received
         MessageRef nextMsg;
         while(qReceiver.RemoveHead(nextMsg).IsOK())
         {
            printf("\n");
            printf("Received the following Message via TCP:\n");
            nextMsg()->PrintToStream();
         }
      }

      // And finally, if there is buffer space for output on the TCP socket,
      // then we should call DoOutput() on the gateway to allow it to send some data
      if (sm.IsSocketReadyForWrite(gateway.GetDataIO()()->GetWriteSelectSocket().GetFileDescriptor()))
      {
         int numBytesSent;
         while((numBytesSent = gateway.DoOutput()) > 0)
         {
            printf("PlainTextMessageIOGateway send %i bytes of Message data out to the TCP socket.\n", numBytesSent);
         }
      }
   }

   printf("\n");
   printf("Bye!\n");
   printf("\n");

   return 0;
}
