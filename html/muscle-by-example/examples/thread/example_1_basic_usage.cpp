#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "system/Thread.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example program demonstrates basic usage of the muscle::Thread class to spawn a captive thread.\n");
   printf("\n");
}

/** This class will implement my own thread's functionality */
class MyThread : public Thread
{
public:
   MyThread() {/* empty */}

protected:
   virtual status_t MessageReceivedFromOwner(const MessageRef & msgRef, uint32 numLeft)
   {
      if (msgRef())
      {
         printf("MyThread::MessageReceivedFromOwner(): MyThread %p received the following Message from the main thread (with " UINT32_FORMAT_SPEC " Messages still left in our command queue after this one)\n", this, numLeft);
         msgRef()->PrintToStream();

         printf("MyThread internal thread sleeping for 1 second, just to demonstrate the asynchronous nature of things...\n");
         Snooze64(SecondsToMicros(1));
         printf("MyThread internal thread has awoke from its 1-second nap.\n");

         return B_NO_ERROR;
      }
      else
      {
         printf("MyThread::MessageReceivedFromOwner():  Oops, main thread thinks we should shut down now!  Returning B_ERROR to exit.\n");
         return B_ERROR;
      }
   }
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   status_t ret;

   MyThread theThread;
   if (theThread.StartInternalThread().IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Error, couldn't start the internal thread!? [%s]\n", ret());
      return 10;
   }

   printf("Enter a command string to send to the internal thread, or enter 'die' to exit this program.\n");

   char buf[1024];
   while(fgets(buf, sizeof(buf), stdin))
   {
      String s = buf;
      s = s.Trim();  // get rid of any newline

      if (s == "die")
      {
         printf("You entered 'die', exiting!\n");
         break;
      }

      if (s.HasChars())
      {
         printf("Main thread:  Sending message containing [%s] to internal thread.\n", s());

         MessageRef toThread = GetMessageFromPool();
         toThread()->AddString("user_command", s);
         if (theThread.SendMessageToInternalThread(toThread).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "SendMessageToThread() failed!? [%s]\n", ret());
      }
   }

   printf("Shutting down the thread...\n");
   theThread.ShutdownInternalThread();
   
   printf("Bye!\n"); 
   printf("\n");

   return 0;
}
