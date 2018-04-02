#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "zlib/ZLibUtilityFunctions.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates the use of DeflateMessage() and InflateMessage() to make a Message object smaller.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // First let's create an example Message object.
   MessageRef rawMsg = GetMessageFromPool(1234);
   {
      rawMsg()->AddFloat("Pi", 3.14159f);
      rawMsg()->AddString("Description", "This is some descriptive text for my example Message.");
      rawMsg()->AddPoint("gps_loc", Point(1.5f, 2.5f));
      rawMsg()->AddInt32("numbers", 1);
      rawMsg()->AddInt32("numbers", 2);
      rawMsg()->AddInt32("numbers", 3);

      MessageRef subMsg = GetMessageFromPool(6666);
      subMsg()->AddBool("This is a sub-Message", true);
      subMsg()->AddString("Peanut Butter", "Jelly");
      subMsg()->AddString("Chocolate", "Vanilla");
      subMsg()->AddString("Cheese", "Crackers");
      subMsg()->AddData("some_data", B_RAW_TYPE, NULL, 10*1024); // a big raw-data buffer of zeros, just to give the deflater more data to deflate
      rawMsg()->AddMessage("subMessage", subMsg);
   }

   LogTime(MUSCLE_LOG_INFO, "Original Message is:\n");
   rawMsg()->PrintToStream();

   printf("\n");
   LogTime(MUSCLE_LOG_INFO, "Deflated Message is:\n");
   MessageRef deflatedMsg = DeflateMessage(rawMsg, 9);
   deflatedMsg()->PrintToStream();
   
   printf("\n");
   LogTime(MUSCLE_LOG_INFO, "Re-inflated Message is:\n");
   MessageRef reinflatedMsg = InflateMessage(rawMsg);
   reinflatedMsg()->PrintToStream();
  
   return 0;
}
