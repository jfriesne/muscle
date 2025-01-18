#include "support/CheckedDataFlattener.h"
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/ByteBuffer.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates adding big-endian numbers to a ByteBuffer using a CheckedBigEndianDataFlattener\n");
   printf("\n");
}

/* This program demonstrates the use of the CheckedBigEndianDataFlattener class
 * to safely populate a ByteBuffer with big-endian data.  You could alternatively
 * use a LittleEndianDataFlattener or a NativeEndianDataFlattener the same way.
 */
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   ByteBuffer buf;

   CheckedBigEndianDataFlattener flat(buf);
   (void) flat.WriteInt32(1);
   (void) flat.WriteInt32(2);
   (void) flat.WriteInt32(3);
   (void) flat.WriteInt16(4);
   (void) flat.WriteInt16(5);
   (void) flat.WriteFloat(3.14159f);
   (void) flat.WriteCString("howdy");

   if (flat.GetStatus().IsOK())
   {
      printf("Here's the ByteBuffer containing three big-endian int32's, followed by\n");
      printf("2 big-endian int16's, pi as a big-endian float, and finally an ASCII string:\n\n");
      buf.Print();
   }
   else printf("There was an error writing big-endian data into the ByteBuffer!  [%s]\n", flat.GetStatus()());

   printf("\n");
   printf("And now we'll grab that data back out of the buffer and display it:\n");

   BigEndianDataUnflattener unflat(buf);
   printf("First int32 is " INT32_FORMAT_SPEC "\n", unflat.ReadInt32());
   printf("Second int32 is " INT32_FORMAT_SPEC "\n", unflat.ReadInt32());
   printf("Third int32 is " INT32_FORMAT_SPEC "\n", unflat.ReadInt32());
   printf("First int16 is %i\n", unflat.ReadInt16());
   printf("Second int16 is %i\n", unflat.ReadInt16());
   printf("Pi is %f\n", unflat.ReadFloat());
   printf("String is [%s]\n", unflat.ReadCString());

   if (unflat.GetStatus().IsOK())
   {
      printf("Big-endian unflattening completed successfully.\n");
   }
   else printf("Big-endian unflattening encountered an error [%s]\n", unflat.GetStatus()());
   printf("\n");

   return 0;
}
