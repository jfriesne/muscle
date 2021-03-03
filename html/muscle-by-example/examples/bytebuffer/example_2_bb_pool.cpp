#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/ByteBuffer.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates getting a ByteBuffer from the ByteBuffer pool\n");
   printf("\n");
}

/* This little program demonstrates using the byte-buffers pool */
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Grab a 128-byte ByteBuffer from the ByteBuffer pool
   ByteBufferRef bbRef = GetByteBufferFromPool(128);
   if (bbRef())
   {
      uint8 * b = bbRef()->GetBuffer();
      uint32 numBytes = bbRef()->GetNumBytes();
      for (uint32 i=0; i<numBytes; i++) b[i] = (uint8) i;

      bbRef()->PrintToStream();
   }
   else MWARN_OUT_OF_MEMORY;

   printf("\n");

   return 0;
}
