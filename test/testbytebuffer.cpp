/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "dataio/FileDataIO.h"
#include "util/MiscUtilityFunctions.h"
#include "util/ByteBuffer.h"

using namespace muscle;

// Test class, just to exercise the ByteBuffer::*Flat() methods 
class TestFlattenable : public Flattenable
{
public:
   TestFlattenable() : _v1(0), _v2(0.0f) {/* empty */}
   TestFlattenable(int v1, float v2) : _v1(v1), _v2(v2) {/* empty */}

   virtual bool IsFixedSize()     const {return true;}
   virtual uint32 TypeCode()      const {return 0;}
   virtual uint32 FlattenedSize() const {return sizeof(_v1) + sizeof(_v2);}

   virtual void Flatten(uint8 *buffer) const
   {
      muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT32 (_v1)); buffer += sizeof(_v1);
      muscleCopyOut(buffer, B_HOST_TO_LENDIAN_IFLOAT(_v2)); //buffer += sizeof(_v2);
   }

   virtual status_t Unflatten(const uint8 *buffer, uint32 size)
   {
      if (size < FlattenedSize()) return B_BAD_DATA; 

      _v1 = B_LENDIAN_TO_HOST_INT32 (muscleCopyIn<int32>(buffer));  buffer += sizeof(_v1);
      _v2 = B_LENDIAN_TO_HOST_IFLOAT(muscleCopyIn<uint32>(buffer)); //buffer += sizeof(_v2);
      return B_NO_ERROR;
   }

   String ToString() const {return String("TestFlattenable: [%1,%2]").Arg(_v1).Arg(_v2);}

private:
   int32 _v1;
   float _v2;
};

static void Test(EndianFlags endianFlags)
{
   ByteBuffer b;
   b.SetEndianFlags(endianFlags);
   printf("Test endianFlags=[%s] ----- sizeof(ByteBuffer)=%i, endian-swap is %s\n", endianFlags.ToHexString()(), (int) sizeof(b), b.IsEndianSwapEnabled()?"Enabled":"Disabled");
   {
      b.AppendInt8(0x01);
      b.AppendInt16(0x0405);
      b.AppendInt32(0x0708090a);
      b.AppendInt64(0x1122334455667788LL);
      b.AppendFloat(3.14159f);
      b.AppendDouble(6.28);
      b.AppendString("Howdy");
      b.AppendString("Pardner");
      b.AppendPoint(Point(-1.1f, -2.2f));
      b.AppendRect(Rect(10.1f, 20.2f, 30.3f, 40.4f));
      b.AppendFlat(TestFlattenable(6, 7.5f));
      b.AppendString("----");
      const int8  i8s[]  = {1,2,3,4};     b.AppendInt8s(i8s, ARRAYITEMS(i8s));
      const int16 i16s[] = {5,6,7,8};     b.AppendInt16s(i16s, ARRAYITEMS(i16s));
      const int32 i32s[] = {9,10,11,12};  b.AppendInt32s(i32s, ARRAYITEMS(i32s));
      const int64 i64s[] = {13,14,15,16}; b.AppendInt64s(i64s, ARRAYITEMS(i64s));
      const float ifls[] = {17.9,18.9,19.9,20.9}; b.AppendFloats(ifls, ARRAYITEMS(ifls));
      const double idbs[] = {21.9,22.9,23.9,24.9}; b.AppendDoubles(idbs, ARRAYITEMS(idbs));
      const String strs[] = {"25", "26", "27", "28"}; b.AppendStrings(strs, ARRAYITEMS(strs));
      const Point pts[] = {Point(29,30),Point(31,32),Point(32,33),Point(33,34)}; b.AppendPoints(pts, ARRAYITEMS(pts));
      const Rect rcs[] = {Rect(35,36,37,38),Rect(39,40,41,42)}; b.AppendRects(rcs, ARRAYITEMS(rcs));
   }

   PrintHexBytes(b);

   uint32 offset = 0, nr;
   printf("int8=0x%x\n", b.ReadInt8(offset));
   printf("int16=0x%x\n", b.ReadInt16(offset));
   printf("int32=0x" XINT32_FORMAT_SPEC "\n", b.ReadInt32(offset));
   printf("int64=0x" XINT64_FORMAT_SPEC "\n", b.ReadInt64(offset));
   printf("float=%f\n", b.ReadFloat(offset));
   printf("double=%f\n", b.ReadDouble(offset));
   printf("string1=[%s]\n", b.ReadString(offset)());
   printf("string2=[%s]\n", b.ReadString(offset)());

   Point p = b.ReadPoint(offset); printf("Point=%f,%f\n", p.x(), p.y());
   Rect r = b.ReadRect(offset); printf("Rect=%f,%f,%f,%f\n", r.left(), r.top(), r.Width(), r.Height());

   TestFlattenable tf;
   if (b.ReadFlat(tf, offset).IsOK()) printf("Flat=[%s]\n", tf.ToString()());
                                 else printf("ReadFlat() failed!?\n");

   printf("string3=[%s]\n", b.ReadString(offset)());  // should be "----"

   int8 i8s[4] = {0}; nr = b.ReadInt8s(i8s, ARRAYITEMS(i8s), offset); 
   printf("i8s=");
   for (uint32 i=0; i<nr; i++) printf(" %i", i8s[i]);
   printf("\n");

   int16 i16s[4] = {0}; nr = b.ReadInt16s(i16s, ARRAYITEMS(i16s), offset);
   printf("i16s=");
   for (uint32 i=0; i<nr; i++) printf(" %i", i16s[i]);
   printf("\n");

   int32 i32s[4] = {0}; nr = b.ReadInt32s(i32s, ARRAYITEMS(i32s), offset);
   printf("i32s=");
   for (uint32 i=0; i<nr; i++) printf(" " INT32_FORMAT_SPEC, i32s[i]);
   printf("\n");

   int64 i64s[4] = {0}; nr = b.ReadInt64s(i64s, ARRAYITEMS(i64s), offset);
   printf("i64s=");
   for (uint32 i=0; i<nr; i++) printf(" " INT64_FORMAT_SPEC, i64s[i]);
   printf("\n");

   float ifls[4] = {0}; nr = b.ReadFloats(ifls, ARRAYITEMS(ifls), offset);
   printf("ifls=");
   for (uint32 i=0; i<nr; i++) printf(" %f", ifls[i]);
   printf("\n");

   double idbs[4] = {0}; nr = b.ReadDoubles(idbs, ARRAYITEMS(idbs), offset);
   printf("idbs=");
   for (uint32 i=0; i<nr; i++) printf(" %f", idbs[i]);
   printf("\n");

   String strs[4]; nr = b.ReadStrings(strs, ARRAYITEMS(strs), offset);
   printf("strs=");
   for (uint32 i=0; i<nr; i++) printf(" [%s]", strs[i]());
   printf("\n");

   Point pts[4]; nr = b.ReadPoints(pts, ARRAYITEMS(pts), offset);
   printf("pts=");
   for (uint32 i=0; i<nr; i++) printf(" [%f,%f]", pts[i][0], pts[i][1]);
   printf("\n");

   Rect rcs[4]; nr = b.ReadRects(rcs, ARRAYITEMS(rcs), offset);
   printf("rcs=");
   for (uint32 i=0; i<nr; i++) printf(" [%f,%f,%f,%f]", rcs[i][0], rcs[i][1], rcs[i][2], rcs[i][3]);
   printf("\n");
}

// This program exercises the ByteBuffer class.
int main(int argc, char ** argv) 
{
   if (argc > 1)
   {
      const char * fileName = argv[1];
      FILE * f = muscleFopen(fileName, "rb");
      if (f)
      {
         FileDataIO fdio(f);
         ByteBufferRef buf = GetByteBufferFromPool(fdio);
         if (buf())
         {
            printf("File [%s] is " UINT32_FORMAT_SPEC " bytes long.  Contents of the file are as follows:\n", fileName, buf()->GetNumBytes());
            PrintHexBytes(buf);
         }
         else printf("Error reading file [%s]\n", fileName);
      }
      else printf("Error, couldn't open file [%s] for reading\n", fileName);
   }
   else
   {
      Test(EndianFlags());  // native-endian
      Test(EndianFlags(ENDIAN_FLAG_FORCE_LITTLE));
      Test(EndianFlags(ENDIAN_FLAG_FORCE_BIG));
   }
   return 0;
}
