/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "dataio/FileDataIO.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"
#include "util/ByteBuffer.h"
#include "util/DataFlattener.h"
#include "util/DataUnflattener.h"

using namespace muscle;

// Test class, just to exercise the ByteBuffer::*Flat() methods
class TestFlattenable : public Flattenable
{
public:
   TestFlattenable() : _v1(0), _v2(0.0f) {/* empty */}
   TestFlattenable(const String & s1, int v1, float v2) : _s1(s1), _v1(v1), _v2(v2) {/* empty */}

   virtual bool IsFixedSize()     const {return false;}
   virtual uint32 TypeCode()      const {return 0;}
   virtual uint32 FlattenedSize() const {return _s1.FlattenedSize() + sizeof(_v1) + sizeof(_v2);}

   virtual void Flatten(uint8 * buffer) const
   {
      DataFlattener h(buffer, MUSCLE_NO_LIMIT);
      h.WriteString(_s1);
      h.WriteInt32(_v1);
      h.WriteFloat(_v2);
   }

   virtual status_t Unflatten(const uint8 *buffer, uint32 size)
   {
      DataUnflattener h(buffer, size);
      _s1 = h.ReadString();
      _v1 = h.ReadInt32();
      _v2 = h.ReadFloat();
      return h.GetStatus();
   }

   String ToString() const {return String("TestFlattenable: [%1,%2,%3]").Arg(_s1).Arg(_v1).Arg(_v2);}

private:
   String _s1;
   int32 _v1;
   float _v2;
};

template<class EndianEncoder> status_t TestHelpers()
{
   uint8 buf[300];  // we're actually using 286 of these, last I checked
   uint32 numValidBytesInBuf = 0;

   // Write out some POD data into (buf)
   {
      DataFlattenerHelper<EndianEncoder> bfh(buf, sizeof(buf));
      bfh.WriteInt8(0x01);
      bfh.WriteInt16(0x0405);
      bfh.WriteInt32(0x0708090a);
      bfh.WriteInt64(0x1122334455667788LL);
      bfh.WriteFloat(3.14159f);
      bfh.WriteDouble(6.28);
      bfh.WriteString("Howdy");
      bfh.WriteCString("Pardner");
      bfh.WriteFlat(Point(-1.1f, -2.2f));
      bfh.WriteFlat(Rect(10.1f, 20.2f, 30.3f, 40.4f));
      bfh.WriteFlat(TestFlattenable("bar", 6, 7.5f));
      bfh.WriteString("----");
      const int8  i8s[]   = {1,2,3,4};                                             bfh.WriteInt8s(   i8s, ARRAYITEMS(i8s));
      const int16 i16s[]  = {5,6,7,8};                                             bfh.WriteInt16s( i16s, ARRAYITEMS(i16s));
      const int32 i32s[]  = {9,10,11,12};                                          bfh.WriteInt32s( i32s, ARRAYITEMS(i32s));
      const int64 i64s[]  = {13,14,15,16};                                         bfh.WriteInt64s( i64s, ARRAYITEMS(i64s));
      const float ifls[]  = {17.9f,18.9f,19.9f,20.9f};                             bfh.WriteFloats( ifls, ARRAYITEMS(ifls));
      const double idbs[] = {21.9,22.9,23.9,24.9};                                 bfh.WriteDoubles(idbs, ARRAYITEMS(idbs));
      const String strs[] = {"25", "26", "27", "28"};                              bfh.WriteStrings(strs, ARRAYITEMS(strs));
      const Point pts[]   = {Point(29,30),Point(31,32),Point(32,33),Point(33,34)}; bfh.WriteFlats(   pts, ARRAYITEMS(pts));
      const Rect rcs[]    = {Rect(35,36,37,38),Rect(39,40,41,42)};                 bfh.WriteFlats(   rcs, ARRAYITEMS(rcs));

      numValidBytesInBuf = bfh.GetNumBytesWritten();
   }

   // Print out the serialized bytes in hexadecimal, so we can see how they were written
   PrintHexBytes(buf, numValidBytesInBuf);

   // Read the serialized bytes back in as POD data again so we can verify it is the same as before
   DataUnflattenerHelper<EndianEncoder> buh(buf, numValidBytesInBuf);

   printf("int8=0x%x\n",                      buh.ReadInt8());
   printf("int16=0x%x\n",                     buh.ReadInt16());
   printf("int32=0x" XINT32_FORMAT_SPEC "\n", buh.ReadInt32());
   printf("int64=0x" XINT64_FORMAT_SPEC "\n", buh.ReadInt64());
   printf("float=%f\n",                       buh.ReadFloat());
   printf("double=%f\n",                      buh.ReadDouble());
   printf("string1=[%s]\n",                   buh.ReadString()());
   printf("string2=[%s]\n",                   buh.ReadCString());

   const Point p = buh.template ReadFlat<Point>(); printf("Point=%f,%f\n", p.x(), p.y());
   const Rect  r = buh.template ReadFlat<Rect>();  printf("Rect=%f,%f,%f,%f\n", r.left(), r.top(), r.Width(), r.Height());

   TestFlattenable tf;
   MRETURN_ON_ERROR(buh.template ReadFlat<TestFlattenable>(tf));

   const String s = buh.ReadString();
   printf("string3=[%s]\n", s());  // should be "----"
   if (s != "----") return B_LOGIC_ERROR("Unexpected string returned by ReadString()!");

   int8 i8s[4] = {0}; MRETURN_ON_ERROR(buh.ReadInt8s(i8s, ARRAYITEMS(i8s)));
   printf("i8s="); for (uint32 i=0; i<ARRAYITEMS(i8s); i++) printf(" %i", i8s[i]); printf("\n");

   int16 i16s[4] = {0}; MRETURN_ON_ERROR(buh.ReadInt16s(i16s, ARRAYITEMS(i16s)));
   printf("i16s="); for (uint32 i=0; i<ARRAYITEMS(i16s); i++) printf(" %i", i16s[i]); printf("\n");

   int32 i32s[4] = {0}; MRETURN_ON_ERROR(buh.ReadInt32s(i32s, ARRAYITEMS(i32s)));
   printf("i32s="); for (uint32 i=0; i<ARRAYITEMS(i32s); i++) printf(" " INT32_FORMAT_SPEC, i32s[i]); printf("\n");

   int64 i64s[4] = {0}; MRETURN_ON_ERROR(buh.ReadInt64s(i64s, ARRAYITEMS(i64s)));
   printf("i64s="); for (uint32 i=0; i<ARRAYITEMS(i64s); i++) printf(" " INT64_FORMAT_SPEC, i64s[i]); printf("\n");

   float ifls[4] = {0}; MRETURN_ON_ERROR(buh.ReadFloats(ifls, ARRAYITEMS(ifls)));
   printf("ifls="); for (uint32 i=0; i<ARRAYITEMS(ifls); i++) printf(" %f", ifls[i]); printf("\n");

   double idbs[4] = {0}; MRETURN_ON_ERROR(buh.ReadDoubles(idbs, ARRAYITEMS(idbs)));
   printf("idbs="); for (uint32 i=0; i<ARRAYITEMS(idbs); i++) printf(" %f", idbs[i]); printf("\n");

   String strs[4]; MRETURN_ON_ERROR(buh.ReadStrings(strs, ARRAYITEMS(strs)));
   printf("strs="); for (uint32 i=0; i<ARRAYITEMS(strs); i++) printf(" [%s]", strs[i]()); printf("\n");

   Point pts[4]; MRETURN_ON_ERROR(buh.ReadFlats(pts, ARRAYITEMS(pts)));
   printf("pts="); for (uint32 i=0; i<ARRAYITEMS(pts); i++) printf(" [%f,%f]", pts[i][0], pts[i][1]); printf("\n");

   Rect rcs[2]; MRETURN_ON_ERROR(buh.ReadFlats(rcs, ARRAYITEMS(rcs)));
   printf("rcs="); for (uint32 i=0; i<ARRAYITEMS(rcs); i++) printf(" [%f,%f,%f,%f]", rcs[i][0], rcs[i][1], rcs[i][2], rcs[i][3]); printf("\n");

   return buh.GetStatus();
}

// This program exercises the ByteBuffer class.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

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
      status_t ret;

      printf("\n\nTesting ByteBufferHelpers with NativeEndianEncoder:\n");
      ret = TestHelpers<NativeEndianEncoder>();
      if (ret.IsError()) LogTime(MUSCLE_LOG_CRITICALERROR, "TestHelpers<NativeEndianEncoder> failed [%s]\n", ret());

      printf("\n\nTesting ByteBufferHelpers with LittleEndianEncoder:\n");
      ret = TestHelpers<LittleEndianEncoder>();
      if (ret.IsError()) LogTime(MUSCLE_LOG_CRITICALERROR, "TestHelpers<LittleEndianEncoder> failed [%s]\n", ret());

      printf("\n\nTesting ByteBufferHelpers with BigEndianEncoder:\n");
      ret = TestHelpers<BigEndianEncoder>();
      if (ret.IsError()) LogTime(MUSCLE_LOG_CRITICALERROR, "TestHelpers<BigEndianEncoder> failed [%s]\n", ret());
   }
   return 0;
}
