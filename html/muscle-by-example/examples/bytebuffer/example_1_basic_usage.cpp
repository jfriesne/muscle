#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/ByteBuffer.h"

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This program demonstrates the basic usage of a ByteBuffer object to hold binary bytes\n");
   p.printf("\n");
}

/* This little program demonstrates basic usage of the muscle::ByteBuffer class */
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

   printf("Example ByteBuffer containing 64 bytes...\n");

   ByteBuffer buf(64);  // 64-byte ByteBuffer
   uint8 * b = buf.GetBuffer();
   if (b)  // just to reassure Coverity
   {
      for (uint32 i=0; i<buf.GetNumBytes(); i++) b[i] = i;
   }
   buf.Print(stdout); // let's view its contents

   printf("\n");
   printf("Appending 10 more bytes to it...\n");
   for (uint32 i=0; i<10; i++) buf += (uint8) i;
   buf.Print(stdout);

   printf("\n");
   printf("Resizing it up to 128 bytes...\n");
   (void) buf.SetNumBytes(128, true);  // true == retain existing data in the buffer
   buf.Print(stdout);

   return 0;
}
