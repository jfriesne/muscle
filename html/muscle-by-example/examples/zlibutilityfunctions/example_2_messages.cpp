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
      (void) rawMsg()->AddFloat("Pi", 3.14159f);
      (void) rawMsg()->AddString("Description", "This is some descriptive text for my example Message.");
      (void) rawMsg()->AddPoint("gps_loc", Point(1.5f, 2.5f));
      (void) rawMsg()->AddInt32("numbers", 1);
      (void) rawMsg()->AddInt32("numbers", 2);
      (void) rawMsg()->AddInt32("numbers", 3);

      MessageRef subMsg = GetMessageFromPool(6666);
      (void) subMsg()->AddBool("This is a sub-Message", true);
      (void) subMsg()->AddString("Peanut Butter", "Jelly");
      (void) subMsg()->AddString("Chocolate", "Vanilla");
      (void) subMsg()->AddString("Cheese", "Crackers");
      (void) subMsg()->AddData("some_data", B_RAW_TYPE, NULL, 10*1024); // a big raw-data buffer of zeros, just to give the deflater more data to deflate
      (void) rawMsg()->AddMessage("subMessage", subMsg);
   }

   LogTime(MUSCLE_LOG_INFO, "Original Message is:\n");
   rawMsg()->Print(stdout);

   printf("\n");
   LogTime(MUSCLE_LOG_INFO, "Deflated Message is:\n");
   ConstMessageRef deflatedMsg = DeflateMessage(rawMsg, 9);
   deflatedMsg()->Print(stdout);

   printf("\n");
   LogTime(MUSCLE_LOG_INFO, "Re-inflated Message is:\n");
   ConstMessageRef reinflatedMsg = InflateMessage(rawMsg);
   reinflatedMsg()->Print(stdout);

   return 0;
}
