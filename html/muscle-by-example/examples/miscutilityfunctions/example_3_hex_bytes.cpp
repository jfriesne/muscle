#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/MiscUtilityFunctions.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates various methods for viewing raw binary data\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();
   
   const uint8 myBuf[] = "This is a buffer of raw binary data.  It just so happens to also be ASCII text, but we will ignore that happy coincidence for now -- it could just as well be any 8-bit bytes.";

   printf("Here is our buffer of data printed as ASCII text:  [%s]\n", myBuf);

   printf("\n"); 
   String hexBytesStr = HexBytesToString(myBuf, sizeof(myBuf));
   printf("And now, here it is as rendered by HexBytesToString():  [%s]\n", hexBytesStr());

   printf("\n");
   ByteBufferRef parseHexBytesStr = ParseHexBytes(hexBytesStr());
   if (parseHexBytesStr()) printf("Here's the result of parsing that previous string back using ParseHexBytes(): [%s]\n", parseHexBytesStr()->GetBuffer());
                      else printf("ParseHexBytes() failed!?\n");

   printf("\n"); 
   String nybbleizedBytes;
   if (NybbleizeData(myBuf, sizeof(myBuf), nybbleizedBytes).IsOK())
   {
      printf("Here it is as rendered into nybblized-data by NybbleizeData():  [%s]\n", nybbleizedBytes());

      ByteBuffer denybbleizedBytes;
      if (DenybbleizeData(nybbleizedBytes, denybbleizedBytes).IsOK())
      {
         printf("And here we've decoded it again with DenybbleizeData():  [%s]\n", denybbleizedBytes());
      }
      else printf("DenybbleizeData() failed!?\n");
   }
   else printf("NybbleizeData() failed!?\n");

   printf("\n"); 
   printf("And finally, our buffer printed out with annotation using PrintHexBytes():\n");
   printf("\n");
   PrintHexBytes(myBuf, sizeof(myBuf));
   printf("\n");

   return 0;
}
