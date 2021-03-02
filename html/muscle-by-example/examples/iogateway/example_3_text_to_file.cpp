#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "message/Message.h"
#include "dataio/FileDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates using a PlainTextMessageIOGateway to write\n");
   printf("a stream of text lines to a file and then read them back in and\n");
   printf("print them out.\n");
   printf("\n");
   printf("Granted this is not the easiest way to accomplish this task; I'm doing it this\n");
   printf("way just to demonstrate how the PlainTextMessageIOGateway class works.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // This scope is the "output some data" phase of this program
   {
      // Set up a FileDataIO so we can output our stream to a file
      FileDataIO fileOutput(fopen("example_3_output.txt", "w"));
      if (fileOutput.GetFile() == NULL)
      {
         printf("Error, couldn't open example_3_output.txt for writing!\n");
         return 10;
      }   

      // Set up our gateway to use the FileDataIO
      PlainTextMessageIOGateway outputGateway;
      outputGateway.SetDataIO(DataIORef(&fileOutput, false));  // false == don't delete pointer!

      // Create a couple of dummy Messages to test with
      MessageRef msg1 = GetMessageFromPool(PR_COMMAND_TEXT_STRINGS);
      msg1()->AddString(PR_NAME_TEXT_LINE, "This is a line of text.");
      msg1()->AddString(PR_NAME_TEXT_LINE, "There are many like it.");
      msg1()->AddString(PR_NAME_TEXT_LINE, "But this one is mine.");

      MessageRef msg2 = GetMessageFromPool(PR_COMMAND_TEXT_STRINGS);
      msg1()->AddString(PR_NAME_TEXT_LINE, "Here is some more text");
      msg1()->AddString(PR_NAME_TEXT_LINE, "From the second Message");

      // Queue up the Messages for output
      (void) outputGateway.AddOutgoingMessage(msg1);
      (void) outputGateway.AddOutgoingMessage(msg2);

      // Turn the crank to make the sausage
      printf("Outputting some text-stream data to example_3_output.txt ...\n");
      while(outputGateway.DoOutput() > 0) {/* empty */} 
   }

   printf("\n");

   // Now let's read the data back in and reconstitute the Messages from it
   {
      // Set up a FileDataIO so we can read our stream in from a file
      FileDataIO fileInput(fopen("example_3_output.txt", "r"));
      if (fileInput.GetFile() == NULL)
      {
         printf("Error, couldn't open example_3_output.txt for readingk!\n");
         return 10;
      }   

      // Set up our gateway to use the FileDataIO
      PlainTextMessageIOGateway inputGateway;
      inputGateway.SetDataIO(DataIORef(&fileInput, false));  // false == don't delete pointer!

      // Turn the crank to eat the sausage
      QueueGatewayMessageReceiver qReceiver;  // we'll collect the parsed Messages in this
      printf("Read some text-stream data from example_3_output.txt ...\n");
      while(inputGateway.DoInput(qReceiver) > 0) {/* empty */} 

      // And finally, we'll print out the Messages that our gateway read in
      printf("Here are the Messages I read back in from example_3_output.txt:\n");
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
