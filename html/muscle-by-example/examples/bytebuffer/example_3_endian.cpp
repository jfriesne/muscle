#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/ByteBuffer.h"
#include "util/DataFlattener.h"
#include "util/DataUnflattener.h"

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

   CheckedBigEndianDataFlattener flt(buf);
   (void) flt.WriteInt32(1);
   (void) flt.WriteInt32(2);
   (void) flt.WriteInt32(3);
   (void) flt.WriteInt16(4);
   (void) flt.WriteInt16(5);
   (void) flt.WriteFloat(3.14159f);
   (void) flt.WriteString("howdy");

   if (flt.GetStatus().IsOK())
   {
      printf("Here's the ByteBuffer containing three big-endian int32's, followed by\n");
      printf("2 big-endian int16's, pi as a big-endian float, and finally an ASCII string:\n\n");
      buf.PrintToStream();
   }
   else printf("There was an error writing big-endian data into the ByteBuffer!  [%s]\n", flt.GetStatus()());

   printf("\n");
   printf("And now we'll grab that data back out of the buffer and display it:\n");

   BigEndianDataUnflattener uflt(buf);
   printf("First int32 is " INT32_FORMAT_SPEC "\n", uflt.ReadInt32());
   printf("Second int32 is " INT32_FORMAT_SPEC "\n", uflt.ReadInt32());
   printf("Third int32 is " INT32_FORMAT_SPEC "\n", uflt.ReadInt32());
   printf("First int16 is %i\n", uflt.ReadInt16());
   printf("Second int16 is %i\n", uflt.ReadInt16());
   printf("Pi is %f\n", uflt.ReadFloat());
   printf("String is [%s]\n", uflt.ReadCString());

   if (uflt.GetStatus().IsOK())
   {
      printf("Big-endian unflattening completed successfully.\n");
   }
   else printf("Big-endian unflattening encountered an error [%s]\n", uflt.GetStatus()());
   printf("\n");

   return 0;
}
