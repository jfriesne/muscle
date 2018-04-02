#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/ByteBuffer.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates the basic usage of a ByteBuffer object to hold binary bytes\n");
   printf("\n");
}

/* This little program demonstrates basic usage of the muscle::ByteBuffer class */
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("Example ByteBuffer containing 64 bytes...\n");
   ByteBuffer buf(64);  // 64-byte ByteBuffer
   uint8 * b = buf.GetBuffer();
   for (uint32 i=0; i<buf.GetNumBytes(); i++) b[i] = i;
   buf.PrintToStream(); // let's view its contents

   printf("\n");
   printf("Appending 10 more bytes to it...\n");
   for (uint32 i=0; i<10; i++) buf += (uint8) i;
   buf.PrintToStream();
   
   printf("\n");
   printf("Resizing it up to 128 bytes...\n");
   (void) buf.SetNumBytes(128, true);  // true == retain existing data in the buffer
   buf.PrintToStream();

   return 0;
}
