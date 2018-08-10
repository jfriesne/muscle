#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/ByteBuffer.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates adding big-endian numbers to a ByteBuffer using ByteBuffer's convenience-methods\n");
   printf("\n");
}

/* This program demonstrates the use of ByteBuffer's datatype-specific
 * convenience methods to populate a ByteBuffer with big-endian data.
 */
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   ByteBuffer buf;
   buf.SetEndianFlags(EndianFlags(ENDIAN_FLAG_FORCE_BIG));  // we want this buffer to hold big-endian data, because reasons

   buf.AppendInt32(1);
   buf.AppendInt32(2);
   buf.AppendInt32(3);
   buf.AppendInt16(4);
   buf.AppendInt16(5);
   buf.AppendFloat(3.14159f);

   printf("Here's the ByteBuffer containing three big-endian int32's, followed by 2 big-endian int16's, and pi as a float:\n");
   buf.PrintToStream();

   printf("\n");
   printf("And now we'll grab that data back out of the buffer and display it:\n");
   uint32 offset = 0;  // will be increased by each call to buf.Read*()
   printf("First int32 is " INT32_FORMAT_SPEC "\n", buf.ReadInt32(offset));
   printf("Second int32 is " INT32_FORMAT_SPEC "\n", buf.ReadInt32(offset));
   printf("Third int32 is " INT32_FORMAT_SPEC "\n", buf.ReadInt32(offset));
   printf("First int16 is %i\n", buf.ReadInt16(offset));
   printf("Second int16 is %i\n", buf.ReadInt16(offset));
   printf("Pi is %f\n", buf.ReadFloat(offset));
   printf("\n");
}
