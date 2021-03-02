#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "message/Message.h"
#include "dataio/FileDataIO.h"
#include "iogateway/MessageIOGateway.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates using a MessageIOGateway to write\n");
   printf("a stream of Messages to a file and then read them back in and\n");
   printf("print them out.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // This scope is the "output some data" phase of this program
   {
      // Set up a FileDataIO so we can output our stream to a file
      FileDataIO fileOutput(fopen("example_1_output.bin", "wb"));
      if (fileOutput.GetFile() == NULL)
      {
         printf("Error, couldn't open example_1_output.bin for writing!\n");
         return 10;
      }   

      // Set up our gateway to use the FileDataIO
      MessageIOGateway outputGateway;
      outputGateway.SetDataIO(DataIORef(&fileOutput, false));  // false == don't delete pointer!

      // Create a couple of dummy Messages to test with
      MessageRef msg1 = GetMessageFromPool(1234);
      msg1()->AddString("Hi there", "everybody");
      msg1()->AddFloat("pi", 3.14159);
      msg1()->AddPoint("los angeles GPS", Point(34.0522f, 118.2437f));

      MessageRef msg2 = GetMessageFromPool(2345);
      msg2()->AddInt32("three+three", 6);
      msg2()->AddInt32("four+four", 8);

      // Queue up the Messages for output
      (void) outputGateway.AddOutgoingMessage(msg1);
      (void) outputGateway.AddOutgoingMessage(msg2);

      // Turn the crank to make the sausage
      printf("Outputting some Message-stream data to example_1_output.bin ...\n");
      while(outputGateway.DoOutput() > 0) {/* empty */} 
   }

   printf("\n");

   // Now let's read the file's data back in and reconstitute the Messages from it
   {
      // Set up a FileDataIO so we can read our stream in from a file
      FileDataIO fileInput(fopen("example_1_output.bin", "rb"));
      if (fileInput.GetFile() == NULL)
      {
         printf("Error, couldn't open example_1_output.bin for readingk!\n");
         return 10;
      }   

      // Set up our gateway to use the FileDataIO
      MessageIOGateway inputGateway;
      inputGateway.SetDataIO(DataIORef(&fileInput, false));  // false == don't delete pointer!

      // Turn the crank to eat the sausage
      QueueGatewayMessageReceiver qReceiver;  // we'll collect the parsed Messages in this
      printf("Read some Message-stream data from example_1_output.bin ...\n");
      while(inputGateway.DoInput(qReceiver) > 0) {/* empty */} 

      // And finally, we'll print out the Messages that our gateway read in
      printf("Here are the Messages I read back in from example_1_output.bin:\n");
      MessageRef nextMsg;
      while(qReceiver.RemoveHead(nextMsg).IsOK())
      {
         printf("\n");
         nextMsg()->PrintToStream();
      }
   }

   printf("\n");

   return 0;
}
