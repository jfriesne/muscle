/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "message/Message.h"
#include "micromessage/MicroMessage.h"
#include "util/MiscUtilityFunctions.h"  // for PrintHexBytes()

using namespace muscle;

static void CreateTestMessage(uint32 recurseCount, Message & m, UMessage * um)
{
   const uint32 ITEM_COUNT = 5;
   uint32 i;

   // Test every type....
   {
      for (i=0; i<ITEM_COUNT; i++)
      {
         const bool b = ((i%2) != 0);
         if (UMAddBool(um, "testBools", b?UTrue:UFalse) != CB_NO_ERROR) printf("UMAddBool(%i) failed!\n", b);
         m.AddBool("testBools", b);
      }
   }
   {
      for (i=0; i<ITEM_COUNT; i++)
      {
         if (UMAddInt8(um, "testInt8s", i) != CB_NO_ERROR) printf("UMAddInt8(" UINT32_FORMAT_SPEC ") failed!\n", i);
         m.AddInt8("testInt8s", i);
      }
   }
   {
      for (i=0; i<ITEM_COUNT; i++)
      {
         if (UMAddInt16(um, "testInt16s", i) != CB_NO_ERROR) printf("UMAddInt16(" UINT32_FORMAT_SPEC ") failed!\n", i);
         m.AddInt16("testInt16s", i);
      }
   }
   {
      for (i=0; i<ITEM_COUNT; i++)
      {
         if (UMAddInt32(um, "testInt32s", i) != CB_NO_ERROR) printf("UMAddInt32(" UINT32_FORMAT_SPEC ") failed!\n", i);
         m.AddInt32("testInt32s", i);
      }
   }
   {
      for (i=0; i<ITEM_COUNT; i++)
      {
         if (UMAddInt64(um, "testInt64s", i) != CB_NO_ERROR) printf("UMAddInt64(" UINT32_FORMAT_SPEC ") failed!\n", i);
         m.AddInt64("testInt64s", i);
      }
   }
   {
      for (i=0; i<ITEM_COUNT; i++)
      {
         const float v = i/10.0f;
         if (UMAddFloat(um, "testFloats", v) != CB_NO_ERROR) printf("UMAddFloat(%f) failed!\n", v);
         m.AddFloat("testFloats", v);
      }
   }
   {
      for (i=0; i<ITEM_COUNT; i++)
      {
         const double v = i/100.0f;
         if (UMAddDouble(um, "testDoubles", v) != CB_NO_ERROR) printf("UMAddDouble(%f) failed!\n", v);
         m.AddDouble("testDoubles", v);
      }
   }
   {
      for (i=0; i<ITEM_COUNT; i++)
      {
         UPoint p;
         p.x = i*10.0f;
         p.y = i*100.0f;
         if (UMAddPoint(um, "testPoints", p) != CB_NO_ERROR) printf("UMAddPoint(%f,%f) failed!\n", p.x, p.y);
         m.AddPoint("testPoints", Point(p.x, p.y));
      }
   }
   {
      for (i=0; i<ITEM_COUNT; i++)
      {
         URect r;
         r.left    = i*10.0f;
         r.top     = i*100.0f;
         r.right   = i*1000.0f;
         r.bottom  = i*10000.0f;
         if (UMAddRect(um, "testRects", r) != CB_NO_ERROR) printf("UMAddRect(%f,%f,%f,%f) failed!\n", r.left, r.top, r.right, r.bottom);
         m.AddRect("testRects", Rect(r.left, r.top, r.right, r.bottom));
      }
   }
   {
      for (i=0; i<ITEM_COUNT; i++)
      {
         char buf[128+ITEM_COUNT];
         muscleSprintf(buf, "This is test string #" UINT32_FORMAT_SPEC " ", i);
         char * b = strchr(buf, '\0');
         for (uint32 j=0; j<i; j++) *b++ = 'A';
         *b = '\0';

         if (UMAddString(um, "testStrings", buf) != CB_NO_ERROR) printf("UMAddString(%s) failed!\n", buf);
         m.AddString("testStrings", buf);
      }
   }
   {
      // Test of out-of-line sub-Message importing
      for (i=0; i<ITEM_COUNT; i++) 
      {
         if (recurseCount > 0)
         {
            Message subMsg(i);
            uint8 subBuf[16384]; UMessage uSubMsg; UMInitializeToEmptyMessage(&uSubMsg, subBuf, sizeof(subBuf), i);
            CreateTestMessage(recurseCount-1, subMsg, &uSubMsg);
            if (UMAddMessage(um, "testMessages", uSubMsg) != CB_NO_ERROR) printf("UMAddMessage() failed!\n");
            m.AddMessage("testMessages", subMsg);
         }
         else 
         {
            uint8 subBuf[12]; UMessage uSubMsg; UMInitializeToEmptyMessage(&uSubMsg, subBuf, sizeof(subBuf), i);
            if (UMAddMessage(um, "testMessages", uSubMsg) != CB_NO_ERROR) printf("Trivial UMAddMessage() failed!\n");
            m.AddMessage("testMessages", Message(i));
         }
      }
   }
   {
      // Test of in-line Message adddition
      for (i=0; i<ITEM_COUNT; i++) 
      {
         if (recurseCount > 0)
         {
            Message subMsg(i+100);
            UMessage uSubMsg = UMInlineAddMessage(um, "inline_Messages", i+100);
            if (UMIsMessageReadOnly(&uSubMsg)) printf("Error, UMInlineAddMessage() failed!\n");
            else
            {
               CreateTestMessage(recurseCount-1, subMsg, &uSubMsg);
               m.AddMessage("inline_Messages", subMsg);
            }
         }
         else 
         {
            UMessage uSubMsg = UMInlineAddMessage(um, "inline_Messages", i+1000);
            if (UMIsMessageReadOnly(&uSubMsg)) printf("Error, trivial UMInlineAddMessage() failed!\n");
                                          else m.AddMessage("inline_Messages", Message(i+1000));
         }
      }
   }
   {
      for (i=0; i<ITEM_COUNT; i++)
      {
         char buf[128+ITEM_COUNT];
         muscleSprintf(buf, "This is test data #" UINT32_FORMAT_SPEC " ", i);
         char * b = strchr(buf, '\0');
         for (uint32 j=0; j<i; j++) *b++ = 'B';
         *b = '\0';

         UMAddData(um, "testDatas", 0x666, buf, ((uint32)strlen(buf))+1);
         m.AddData(    "testDatas", 0x666, buf, ((uint32)strlen(buf))+1);
      }
   }
}

// This program compares flattened UMessages against flattened Messages, to
// make sure the created bytes are the same in both cases.
int main(int, char **)
{
   Message m(0x1234);
   uint8 umBuf[256*1024]; UMessage um; UMInitializeToEmptyMessage(&um, umBuf, sizeof(umBuf), 0x1234);
   CreateTestMessage(2, m, &um);

   printf("\n---------------------------------UMsg:\n");
   UMPrintToStream(&um, NULL);

   printf("\n---------------------------------Msg:\n");
   m.PrintToStream();

   printf("\n---------------------------------UMsg:\n");
   const uint8 * umPtr = UMGetFlattenedBuffer(&um);
   const uint32 umFlatSize = UMGetFlattenedSize(&um);
   PrintHexBytes(umPtr, umFlatSize);

   ByteBufferRef bufRef = GetByteBufferFromPool(m.FlattenedSize());
   uint8 * mPtr = bufRef()->GetBuffer();
   const uint32 mFlatSize = bufRef()->GetNumBytes();
   m.Flatten(mPtr);
   printf("\n---------------------------------Msg:\n");
   PrintHexBytes(mPtr, mFlatSize);

        if (umFlatSize != mFlatSize) printf("Flattened buffer sizes didn't match!  UMessage=" UINT32_FORMAT_SPEC " Message=" UINT32_FORMAT_SPEC "\n", umFlatSize, mFlatSize);
   else if (memcmp(mPtr, umPtr, mFlatSize) != 0)
   {
      for (uint32 i=0; i<mFlatSize; i++) 
      {
         if (mPtr[i] != umPtr[i])
         {
            printf("BYTE MISMATCH AT POSITION " UINT32_FORMAT_SPEC ":  Micro=%02x vs Normal=%02x\n", i, umPtr[i], mPtr[i]);
            break;
         }
      }
   }
   else printf("Buffers matched, yay!\n");

   return 0;
}
