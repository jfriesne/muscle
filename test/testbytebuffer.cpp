/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "dataio/FileDataIO.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"
#include "util/ByteBuffer.h"

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

   virtual void Flatten(DataFlattener flat) const
   {
      flat.WriteFlat(_s1);
      flat.WriteInt32(_v1);
      flat.WriteFloat(_v2);
   }

   virtual status_t Unflatten(DataUnflattener & unflat)
   {
      _s1 = unflat.ReadFlat<String>();
      _v1 = unflat.ReadInt32();
      _v2 = unflat.ReadFloat();
      return unflat.GetStatus();
   }

   String ToString() const {return String("TestFlattenable: [%1,%2,%3]").Arg(_s1).Arg(_v1).Arg(_v2);}

private:
   String _s1;
   int32 _v1;
   float _v2;
};

template<class EndianConverter> status_t TestHelpers()
{
   uint8 buf[300];  // we're actually using 286 of these, last I checked
   uint32 numValidBytesInBuf = 0;

   // Write out some POD data into (buf)
   {
      DataFlattenerHelper<EndianConverter> flat(buf, sizeof(buf));
      flat.SetCompleteWriteRequired(false);

      flat.WriteInt8(0x01);
      flat.WriteInt16(0x0405);
      flat.WriteInt32(0x0708090a);
      flat.WriteInt64(0x1122334455667788LL);
      flat.WriteFloat(3.14159f);
      flat.WriteDouble(6.28);
      flat.WriteFlat(String("Howdy"));
      flat.WriteCString("Pardner");
      flat.WriteFlat(Point(-1.1f, -2.2f));
      flat.WriteFlat(Rect(10.1f, 20.2f, 30.3f, 40.4f));
      flat.WriteFlat(TestFlattenable("bar", 6, 7.5f));
      flat.WriteCString("----");
      const int8  i8s[]   = {1,2,3,4};                                             flat.WriteInt8s(   i8s, ARRAYITEMS(i8s));
      const int16 i16s[]  = {5,6,7,8};                                             flat.WriteInt16s( i16s, ARRAYITEMS(i16s));
      const int32 i32s[]  = {9,10,11,12};                                          flat.WriteInt32s( i32s, ARRAYITEMS(i32s));
      const int64 i64s[]  = {13,14,15,16};                                         flat.WriteInt64s( i64s, ARRAYITEMS(i64s));
      const float ifls[]  = {17.9f,18.9f,19.9f,20.9f};                             flat.WriteFloats( ifls, ARRAYITEMS(ifls));
      const double idbs[] = {21.9,22.9,23.9,24.9};                                 flat.WriteDoubles(idbs, ARRAYITEMS(idbs));
      const String strs[] = {"25", "26", "27", "28"};                              flat.WriteFlats(  strs, ARRAYITEMS(strs));
      const Point pts[]   = {Point(29,30),Point(31,32),Point(32,33),Point(33,34)}; flat.WriteFlats(   pts, ARRAYITEMS(pts));
      const Rect rcs[]    = {Rect(35,36,37,38),Rect(39,40,41,42)};                 flat.WriteFlats(   rcs, ARRAYITEMS(rcs));

      flat.WriteCString("9 bytes!");  // 8 ASCII chars plus a NUL byte
      flat.WritePaddingBytesToAlignTo(sizeof(uint32));
      flat.WriteInt32(0x12345678);  // this should be uint32-aligned
      numValidBytesInBuf = flat.GetNumBytesWritten();
   }

   // Print out the serialized bytes in hexadecimal, so we can see how they were written
   PrintHexBytes(stdout, buf, numValidBytesInBuf);

   // Read the serialized bytes back in as POD data again so we can verify it is the same as before
   DataUnflattenerHelper<EndianConverter> unflat(buf, numValidBytesInBuf);

   printf("int8=0x%x\n",                      unflat.ReadInt8());
   printf("int16=0x%x\n",                     unflat.ReadInt16());
   printf("int32=0x" XINT32_FORMAT_SPEC "\n", unflat.ReadInt32());
   printf("int64=0x" XINT64_FORMAT_SPEC "\n", unflat.ReadInt64());
   printf("float=%f\n",                       unflat.ReadFloat());
   printf("double=%f\n",                      unflat.ReadDouble());
   printf("string1=[%s]\n",                   unflat.template ReadFlat<String>()());
   printf("string2=[%s]\n",                   unflat.ReadCString("<NULL>"));

   const Point p = unflat.template ReadFlat<Point>(); printf("Point=%f,%f\n", p.x(), p.y());
   const Rect  r = unflat.template ReadFlat<Rect>();  printf("Rect=%f,%f,%f,%f\n", r.left(), r.top(), r.GetWidth(), r.GetHeight());

   TestFlattenable tf;
   MRETURN_ON_ERROR(unflat.template ReadFlat<TestFlattenable>(tf));

   const String s = unflat.template ReadFlat<String>();
   printf("string3=[%s]\n", s());  // should be "----"
   if (s != "----") return B_LOGIC_ERROR("Unexpected string returned by ReadString()!");

   int8 i8s[4] = {0}; MRETURN_ON_ERROR(unflat.ReadInt8s(i8s, ARRAYITEMS(i8s)));
   printf("i8s="); for (uint32 i=0; i<ARRAYITEMS(i8s); i++) printf(" %i", i8s[i]); printf("\n");

   int16 i16s[4] = {0}; MRETURN_ON_ERROR(unflat.ReadInt16s(i16s, ARRAYITEMS(i16s)));
   printf("i16s="); for (uint32 i=0; i<ARRAYITEMS(i16s); i++) printf(" %i", i16s[i]); printf("\n");

   int32 i32s[4] = {0}; MRETURN_ON_ERROR(unflat.ReadInt32s(i32s, ARRAYITEMS(i32s)));
   printf("i32s="); for (uint32 i=0; i<ARRAYITEMS(i32s); i++) printf(" " INT32_FORMAT_SPEC, i32s[i]); printf("\n");

   int64 i64s[4] = {0}; MRETURN_ON_ERROR(unflat.ReadInt64s(i64s, ARRAYITEMS(i64s)));
   printf("i64s="); for (uint32 i=0; i<ARRAYITEMS(i64s); i++) printf(" " INT64_FORMAT_SPEC, i64s[i]); printf("\n");

   float ifls[4] = {0}; MRETURN_ON_ERROR(unflat.ReadFloats(ifls, ARRAYITEMS(ifls)));
   printf("ifls="); for (uint32 i=0; i<ARRAYITEMS(ifls); i++) printf(" %f", ifls[i]); printf("\n");

   double idbs[4] = {0}; MRETURN_ON_ERROR(unflat.ReadDoubles(idbs, ARRAYITEMS(idbs)));
   printf("idbs="); for (uint32 i=0; i<ARRAYITEMS(idbs); i++) printf(" %f", idbs[i]); printf("\n");

   String strs[4]; MRETURN_ON_ERROR(unflat.ReadFlats(strs, ARRAYITEMS(strs)));
   printf("strs="); for (uint32 i=0; i<ARRAYITEMS(strs); i++) printf(" [%s]", strs[i]()); printf("\n");

   Point pts[4]; MRETURN_ON_ERROR(unflat.ReadFlats(pts, ARRAYITEMS(pts)));
   printf("pts="); for (uint32 i=0; i<ARRAYITEMS(pts); i++) printf(" [%f,%f]", pts[i][0], pts[i][1]); printf("\n");

   Rect rcs[2]; MRETURN_ON_ERROR(unflat.ReadFlats(rcs, ARRAYITEMS(rcs)));
   printf("rcs="); for (uint32 i=0; i<ARRAYITEMS(rcs); i++) printf(" [%f,%f,%f,%f]", rcs[i][0], rcs[i][1], rcs[i][2], rcs[i][3]); printf("\n");

   const char * nineBytes = unflat.ReadCString();
   printf("nineBytes=[%s]\n", nineBytes);

   (void) unflat.SeekPastPaddingBytesToAlignTo(sizeof(uint32));
   printf("Aligned=" XINT32_FORMAT_SPEC "\n", unflat.ReadInt32());

   return unflat.GetStatus();
}

// This program exercises the ByteBuffer class.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   if ((argc > 1)&&(strcmp(argv[1], "fromscript") != 0))
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
            PrintHexBytes(stdout, buf);
            return 0;
         }
         else printf("Error reading file [%s]\n", fileName);
      }
      else printf("Error, couldn't open file [%s] for reading\n", fileName);

      return 10;
   }
   else
   {
      status_t ret;

      printf("\n\nTesting ByteBufferHelpers with LittleEndianConverter:\n");
      ret = TestHelpers<LittleEndianConverter>();
      if (ret.IsError()) LogTime(MUSCLE_LOG_CRITICALERROR, "TestHelpers<LittleEndianConverter> failed [%s]\n", ret());

#ifdef DISABLE_FOR_NOW_AS_THEY_ARENT_CURRENTLY_WORKING
      printf("\n\nTesting ByteBufferHelpers with NativeEndianConverter:\n");
      ret = TestHelpers<NativeEndianConverter>();
      if (ret.IsError()) LogTime(MUSCLE_LOG_CRITICALERROR, "TestHelpers<NativeEndianConverter> failed [%s]\n", ret());

      printf("\n\nTesting ByteBufferHelpers with BigEndianConverter:\n");
      ret = TestHelpers<BigEndianConverter>();
      if (ret.IsError()) LogTime(MUSCLE_LOG_CRITICALERROR, "TestHelpers<BigEndianConverter> failed [%s]\n", ret());
#endif

      return ret.IsOK() ? 0 : 10;
   }
}
