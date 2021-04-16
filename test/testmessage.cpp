/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "message/Message.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"

using namespace muscle;

#define TEST(x) if ((x).IsError()) printf("Operation failed, line %i\n", __LINE__);
#define NEGATIVETEST(x) if ((x).IsOK()) printf("Operation succeeded when it should not have, line %i\n", __LINE__); 

void printSep(const char * title);
void printSep(const char * title)
{
   printf("\n----------------- %s -------------------\n", title);
}

#define COMMAND_HELLO   0x1234
#define COMMAND_GOODBYE 0x4321

// Just a dummy class to test AddFlat()/FindFlat() against
class TestFlatCountable : public FlatCountable
{
public:
   TestFlatCountable()
      : _val(-1) 
   {
      // empty
   }
   TestFlatCountable(const String & s, int32 val) : _string(s), _val(val) {/* empty */}

   bool operator != (const TestFlatCountable & rhs) const {return ((_val != rhs._val)||(_string != rhs._string));}

   virtual bool IsFixedSize() const {return false;}
   virtual uint32 TypeCode() const {return 123456;}
   virtual uint32 FlattenedSize() const {return sizeof(_val)+_string.FlattenedSize();}

   virtual void Flatten(uint8 *buffer) const
   {
      muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT32(_val)); buffer += sizeof(_val);
      _string.Flatten(buffer);
   }

   virtual status_t Unflatten(const uint8 *buf, uint32 size)
   {
      if (size < sizeof(_val)) return B_BAD_DATA;
      _val = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<int32>(buf)); buf += sizeof(_val); size -= sizeof(_val);
      return _string.Unflatten(buf, size);
   }

   String ToString() const {return String("TFC:  [%1] %2").Arg(_string).Arg(_val);}

private:
   String _string;
   int32 _val; 
};
DECLARE_REFTYPES(TestFlatCountable);

static void TestTemplatedFlatten(const Message & m, int lineNumber)
{
   const uint32 oldChecksum = m.CalculateChecksum();
   MessageRef messageTemplate = m.CreateMessageTemplate();
   if (messageTemplate() == NULL)
   {
      printf("CreateMessageTemplate() failed!\n");
      exit(10);
   }

   const uint32 newChecksum = m.CalculateChecksum();
   if (newChecksum != oldChecksum)
   {
      printf("CreateMessageTemplate() caused original Message's checksum to change from " UINT32_FORMAT_SPEC " to " UINT32_FORMAT_SPEC ", that shouldn't happen!\n", oldChecksum, newChecksum);
      exit(10);
   }

   const uint32 templatedFlatSize = m.TemplatedFlattenedSize(*messageTemplate());
   const uint32 regularFlatSize   = m.FlattenedSize();
   printf("TEMPLATE TEST at line %i:  templatedFlatSize=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " (%.0f%% size reduction)\n", lineNumber, templatedFlatSize, regularFlatSize, 100.0*(1.0-((float)templatedFlatSize/regularFlatSize)));

   printf("Message is:\n");
   m.PrintToStream();
 
   ByteBufferRef buf = GetByteBufferFromPool(templatedFlatSize);
   memset(buf()->GetBuffer(), 'X', buf()->GetNumBytes());  // just to make any unwritten-to-bytes more obvious
   m.TemplatedFlatten(*messageTemplate(), buf()->GetBuffer());

   //printf("Template Message is:\n");
   //messageTemplate()->PrintToStream();

   printf("Templated flattened buffer is:\n");
   PrintHexBytes(buf);

   status_t ret;
   Message newMsg;
   if (newMsg.TemplatedUnflatten(*messageTemplate(), buf()->GetBuffer(), buf()->GetNumBytes()).IsOK(ret))
   {
      if (newMsg != m)
      {
         printf("Template test failed (line %i), Unflattened Message didn't match the original!  Unflattened Message is:\n", lineNumber);
         newMsg.PrintToStream();
         exit(10);
      }
   }
   else 
   {
      printf("TemplatedUnflatten() (line %i) failed [%s]\n", lineNumber, ret());
      exit(10);
   }
}

// This program exercises the Message class.
int main(int, char **)
{
   CompleteSetupSystem css;  // required!
   SetConsoleLogLevel(MUSCLE_LOG_DEBUG);  // so if unflatten() fails we can see why

   // Test muscleSwap()
   {
      Message m1(1), m2(2);
      m1.AddString("blah", "m1");
      m2.AddString("blah", "m2");
      PrintAndClearStringCopyCounts("Before muscleSwap()");
      muscleSwap(m1, m2);
      PrintAndClearStringCopyCounts("After muscleSwap()");
      if ((m1.what != 2)||(m2.what != 1)||(m1.GetString("blah") != "m2")||(m2.GetString("blah") != "m1"))
      {
         printf("Oh no, muscleSwap is broken for Message objects!\n");
         exit(10);
      }
   }

   Message m1;
   m1.AddFloat("va", 1.0f);
   TestTemplatedFlatten(m1, __LINE__);
   m1.AddFloat("va", 2.0f);
   TestTemplatedFlatten(m1, __LINE__);
   printf("m1 flattenedSize=" UINT32_FORMAT_SPEC "\n", m1.FlattenedSize());
   m1.AddInt32("co", 32);
   TestTemplatedFlatten(m1, __LINE__);
   printf("m2 flattenedSize=" UINT32_FORMAT_SPEC "\n", m1.FlattenedSize());

   printSep("Testing Replace*() with okayToAdd...");
   Message butter;
   butter.ReplaceInt8(true, "int8", 8);
   butter.ReplaceInt16(true, "int16", 16);
   butter.ReplaceInt32(true, "int32", 32);
   butter.ReplaceInt64(true, "int64", 64);
   butter.ReplaceFloat(true, "float", 3.14f);
   butter.ReplaceDouble(true, "double", 6.28);
   butter.ReplacePoint(true, "point", Point(5,4));
   butter.ReplaceRect(true, "rect", Rect(5,6,7,8));
   TestTemplatedFlatten(butter, __LINE__);
   butter.ReplacePointer(true, "pointer", &butter);
   butter.PrintToStream();

   butter.ReplaceInt16(true, "int16", 0, 17);
   butter.ReplaceInt16(true, "int16", 1, 18);
   butter.ReplaceInt8(true, "int8", 25, 25);  // should work the same as AddInt8("int8", 25);

   butter.AddTag("Tag", RefCountableRef(GetMessageFromPool(6666)()));
   butter.AddTag("Tag", RefCountableRef(GetMessageFromPool(7777)()));
   butter.AddPointer("pointer", &butter);
   butter.PrintToStream();

   void * t;
   if ((butter.FindPointer("pointer", t).IsError())||(t != &butter)) printf("Error retrieving pointer!\n");

   (void) butter.RemoveName("pointer");  // otherwise the templated test will fail since pointer fields don't get flattened
   (void) butter.RemoveName("Tag");      // otherwise the templated test will fail since tag fields don't get flattened
   TestTemplatedFlatten(butter, __LINE__);

   printf("(butter==m1) == %i\n", butter == m1);
   printf("(butter==butter) == %i\n", butter == butter);

   printSep("Testing Add*()...");

   Message msg(COMMAND_HELLO);
   TEST(msg.AddString("Friesner", "Jeremy"));
   TEST(msg.AddString("Friesner", "Joanna"));
   TEST(msg.AddString("Friesner", "Joellen"));
   TEST(msg.AddString("Chicken", "Soup"));
   TEST(msg.AddString("Chicken", "Vegetable"));
   TEST(msg.AddString("Chicken", "Lips"));
   TEST(msg.AddString("Fred", "Flintstone"));
   TEST(msg.AddString("Buddha", "Bark"));
   TEST(msg.AddPoint("point12", Point(1,2)));
   TEST(msg.AddPoint("point12", Point(2,1)));
   TEST(msg.AddRect("rect1234", Rect(1,2,3,4)));
   TEST(msg.AddRect("rect2345", Rect(2,3,4,5)));
   TEST(msg.AddInt8("int8", 45));
   TEST(msg.AddInt16("int16", 123));
   TEST(msg.AddInt32("int32", 89));
   TEST(msg.AddFloat("float", 3.14159f));
   TEST(msg.AddDouble("double", 6.28));
   TEST(msg.AddDouble("double", 6.66));
   TEST(msg.AddMessage("msg", butter));
   TEST(msg.AddInt64("int64", 99999));
   TEST(msg.AddData("Data", B_RAW_TYPE, "ABCDEFGHIJKLMNOPQRS", 12));
   TEST(msg.AddData("Data", B_RAW_TYPE, "Mouse", 3));
   TestTemplatedFlatten(msg, __LINE__);
   TEST(msg.AddPointer("ptr", &msg));
   TEST(msg.AddPointer("ptr", &butter));

   printf("Testing the Get*() functions...\n");
   printf("GetCstr(\"Friesner\")     =%s\n", msg.GetCstr(  "Friesner", "<not found>"));
   printf("GetCstr(\"Friesner(0)\")  =%s\n", msg.GetCstr(  "Friesner", "<not found>", 0));
   printf("GetCstr(\"Friesner(1)\")  =%s\n", msg.GetCstr(  "Friesner", "<not found>", 1));
   printf("GetCstr(\"Friesner(2)\")  =%s\n", msg.GetCstr(  "Friesner", "<not found>", 2));
   printf("GetCstr(\"Friesner(3)\")  =%s\n", msg.GetCstr(  "Friesner", "<not found>", 3));
   printf("GetString(\"Friesner\")   =%s\n", msg.GetString("Friesner", "<not found>")());
   printf("GetString(\"Friesner(0)\")=%s\n", msg.GetString("Friesner", "<not found>", 0)());
   printf("GetString(\"Friesner(1)\")=%s\n", msg.GetString("Friesner", "<not found>", 1)());
   printf("GetString(\"Friesner(2)\")=%s\n", msg.GetString("Friesner", "<not found>", 2)());
   printf("GetString(\"Friesner(3)\")=%s\n", msg.GetString("Friesner", "<not found>", 3)());
   printf("GetInt8=%i\n", msg.GetInt8("int8")); 
   printf("GetInt16=%i\n", msg.GetInt16("int16")); 
   printf("GetInt32=" INT32_FORMAT_SPEC "\n", msg.GetInt32("int32")); 
   printf("GetInt64=" INT64_FORMAT_SPEC "\n", msg.GetInt64("int64")); 
   printf("GetInt64_XXX=" INT64_FORMAT_SPEC "\n", msg.GetInt64("not_present")); 
   printf("GetInt64_666=" INT64_FORMAT_SPEC "\n", msg.GetInt64("not_present", 666)); 
   printf("GetDouble(0)=%f\n", msg.GetDouble("double", 0, 0));
   printf("GetDouble(1)=%f\n", msg.GetDouble("double", 0, 1));
   printf("GetDouble(2)=%f\n", msg.GetDouble("double", 0, 2));
   printf("GetDouble(3)=%f\n", msg.GetDouble("double", 8888.0, 3));
   printf("GetPointer(0)=%p\n", msg.GetPointer("ptr", NULL, 0));
   printf("GetPointer(1)=%p\n", msg.GetPointer("ptr", NULL, 1));
   MessageRef getButter = msg.GetMessage("msg");
   if (getButter()) printf("GetMessage = [%s]\n", getButter()->ToString()()); 
               else printf("GetMessage = NULL\n");

   Message subMessage(1);
   TEST(subMessage.AddString("I am a", "sub message!"));
   TEST(subMessage.AddInt32("My age is", 32));

   Message subsubMessage(2);
   TEST(subsubMessage.AddBool("Wow, that's deep!", true));
   TEST(subsubMessage.AddMessage("This is actually okay to do!", subsubMessage));
   TEST(subMessage.AddMessage("subsubMessage", subsubMessage));

   TEST(msg.AddMessage("subMessage", subMessage));
   
   {for (int i=0; i<10; i++) TEST(msg.AddInt8("TestInt8", i));    }
   {for (int i=0; i<10; i++) TEST(msg.AddInt16("TestInt16", i));  }
   {for (int i=0; i<10; i++) TEST(msg.AddInt32("TestInt32", i));  }
   {for (int i=0; i<10; i++) TEST(msg.AddInt64("TestInt64", i));  }
   {for (int i=0; i<10; i++) TEST(msg.AddDouble("TestDouble", i));}
   {for (int i=0; i<10; i++) TEST(msg.AddFloat("TestFloat", (float)i));  }
   {for (int i=0; i<10; i++) TEST(msg.AddBool("TestBool", (i!=0)));}

   printf("Finished message:\n");
   msg.PrintToStream();

   printSep("Testing RemoveName, RemoveData, Replace*()...");
   TEST(msg.RemoveData("TestInt8", 5));
   TEST(msg.RemoveName("Buddha"));
   TEST(msg.RemoveData("Fred", 0));
   TEST(msg.RemoveData("Friesner", 1));
   NEGATIVETEST(msg.RemoveData("Glorp", 0));  
   NEGATIVETEST(msg.RemoveData("Chicken", 5)); 
   TEST(msg.ReplaceString(false, "Friesner", 0, "Jorge"));
   TEST(msg.ReplaceString(false, "Chicken", 1, "Feet"));
   TEST(msg.ReplaceString(false, "Chicken", 2, "Breast"));
   NEGATIVETEST(msg.ReplaceString(false, "Chicken", 3, "Soul"));  
   TEST(msg.ReplaceDouble(true, "TestDouble", 2, 222.222));
   TEST(msg.ReplaceFloat(true, "TestFloat", 3, 333.333f));
   NEGATIVETEST(msg.ReplaceFloat(false, "RootBeerFloat", 0, 444.444f));
   TEST(msg.ReplaceBool(false, "TestBool", true));
   TEST(msg.ReplaceRect(false, "rect2345", Rect(2,3,4,5)));

   Message eqMsg = msg;
   printf("EQMSG=msg == %i\n", eqMsg==msg);

   printf("Replaced message:\n");
   msg.PrintToStream();

   printSep("Testing the Find() commands...");
   String strResult;
   TEST(msg.FindString("Friesner", strResult));
   printf("Friesner(0) = %s\n", strResult.Cstr());
   const char * res = NULL;
   TEST(msg.FindString("Friesner", 1, res));
   printf("Friesner(1) = %s\n", res);
   NEGATIVETEST(msg.FindString("Friesner", 2, strResult));
   NEGATIVETEST(msg.FindString("Friesner", 3, strResult));  

   int8 int8Result;
   TEST(msg.FindInt8("TestInt8", 5, int8Result));
   printf("TestInt8(5b) = %i\n",int8Result);
   TEST(msg.FindInt8("TestInt8", 5, &int8Result));
   printf("TestInt8(5a) = %i\n", int8Result);

   int16 int16Result;
   TEST(msg.FindInt16("TestInt16", 4, int16Result));
   printf("TestInt16(4a) = %i\n",int16Result);
   TEST(msg.FindInt16("TestInt16", 4, &int16Result));
   printf("TestInt16(4b) = %i\n",int16Result);

   int32 int32Result;
   TEST(msg.FindInt32("TestInt32", 4, int32Result));
   printf("TestInt32(4) = " INT32_FORMAT_SPEC "\n",int32Result);

   uint32 uint32Result;
   TEST(msg.FindInt32("TestInt32", 4, uint32Result));
   printf("TestUInt32(4) = " INT32_FORMAT_SPEC "\n",uint32Result);

   int64 int64Result;
   TEST(msg.FindInt64("TestInt64", 4, int64Result));
   printf("TestInt64(4a) = " INT64_FORMAT_SPEC "\n",int64Result);
   TEST(msg.FindInt64("TestInt64", 4, &int64Result));
   printf("TestInt64(4b) = " INT64_FORMAT_SPEC "\n",int64Result);

   uint64 uint64Result;
   TEST(msg.FindInt64("TestInt64", 4, uint64Result));
   printf("TestUInt64(4a) = " INT64_FORMAT_SPEC "\n",uint64Result);
   TEST(msg.FindInt64("TestInt64", 4, &uint64Result));
   printf("TestUInt64(4b) = " INT64_FORMAT_SPEC "\n",uint64Result);

   float floatResult;
   TEST(msg.FindFloat("TestFloat", 4, floatResult));
   printf("TestFloat(4) = %f\n",floatResult);

   double doubleResult;
   TEST(msg.FindDouble("TestDouble", 4, doubleResult));
   printf("TestDouble(4) = %f\n",doubleResult);

   Rect rectResult;
   TEST(msg.FindRect("rect2345", rectResult));
   printf("TestRect: "); rectResult.PrintToStream();

   Point pointResult;
   TEST(msg.FindPoint("point12", 1, pointResult));
   printf("TestPoint: "); pointResult.PrintToStream();

   msg.AddTag("ThisShouldn'tBeBackAfterUnflatten", RefCountableRef(NULL));

   const void * gd;
   uint32 getDataSize;
   TEST(msg.FindData("Data", B_RAW_TYPE, &gd, &getDataSize));
   String dataStr((const char *) gd, getDataSize);
   printf("data=[%s], size=" UINT32_FORMAT_SPEC "\n", dataStr(), getDataSize);

   TEST(msg.FindData("Data", B_RAW_TYPE, 1, &gd, &getDataSize));
   dataStr.SetCstr((const char *) gd, getDataSize);
   printf("data(1)=[%s], size=" UINT32_FORMAT_SPEC "\n", dataStr(), getDataSize);
  
   printSep("Testing misc");

   printf("There are " UINT32_FORMAT_SPEC " string entries\n", msg.GetNumNames(B_STRING_TYPE));
   msg.PrintToStream();
   Message tryMe = msg;
   printf("Msg is " UINT32_FORMAT_SPEC " bytes.\n",msg.FlattenedSize());
   msg.AddTag("anothertag", RefCountableRef(GetMessageFromPool()()));
   printf("After adding tag, msg is (hopefully still) " UINT32_FORMAT_SPEC " bytes.\n",msg.FlattenedSize());
   tryMe.PrintToStream();

   printf("Extracting...\n");
   Message extract;
   TEST(tryMe.FindMessage("subMessage", extract));
   printSep("Extracted subMessage!\n");
   extract.PrintToStream();

   Message subExtract;
   TEST(extract.FindMessage("subsubMessage", subExtract));
   printSep("Extracted subsubMessage!\n");
   subExtract.PrintToStream();

   const uint32 flatSize = msg.FlattenedSize();
   printf("FlatSize=" UINT32_FORMAT_SPEC "\n",flatSize);
   uint8 * buf = new uint8[flatSize*10];
   {for (uint32 i=flatSize; i<flatSize*10; i++) buf[i] = 'J';}

   msg.Flatten(buf);

   {for (uint32 i=flatSize; i<flatSize*10; i++) if (buf[i] != 'J') printf("OVERWRITE ON BYTE " UINT32_FORMAT_SPEC "\n",i);}
   printf("\n====\n");
   
   PrintHexBytes(buf, flatSize);

   Message copy;
   if (copy.Unflatten(buf, flatSize).IsOK())
   {
      printf("****************************\n");
      copy.PrintToStream();
      printf("***************************2\n");
      Message dup(copy);
      dup.PrintToStream();
   }
   else printf("Rats, Unflatten did not work.  :^(\n");
   delete [] buf;

   printf("Testing field name iterator... B_ANY_TYPE \n");
   for (MessageFieldNameIterator it(copy); it.HasData(); it++) printf("--> [%s]\n", it.GetFieldName()());

   printf("Testing field name iterator... B_ANY_TYPE\n");
   for (MessageFieldNameIterator it = copy.GetFieldNameIterator(B_ANY_TYPE); it.HasData(); it++) printf("--> [%s]\n", it.GetFieldName()());

   printf("Testing field name iterator... B_STRING_TYPE\n");
   for (MessageFieldNameIterator it = copy.GetFieldNameIterator(B_STRING_TYPE); it.HasData(); it++) printf("--> [%s]\n", it.GetFieldName()());

   printf("Testing field name iterator... B_INT8_TYPE\n");
   for (MessageFieldNameIterator it = copy.GetFieldNameIterator(B_INT8_TYPE); it.HasData(); it++) printf("--> [%s]\n", it.GetFieldName()());

   printf("Testing field name iterator... B_OBJECT_TYPE (should have no results)\n");
   for (MessageFieldNameIterator it = copy.GetFieldNameIterator(B_OBJECT_TYPE); it.HasData(); it++) printf("--> [%s]\n", it.GetFieldName()());

   printf("Testing adding and retrieval of FlatCountableRefs by reference\n");
   TestFlatCountableRef tfcRef(new TestFlatCountable("Hello", 5));
   if (msg.AddFlat("tfc", FlatCountableRef(tfcRef.GetRefCountableRef(), false)).IsOK())
   {
      TestFlatCountable tfc2;
      if (msg.FindFlat("tfc", tfc2).IsOK()) 
      {
         printf("FindFlat() found: [%s]\n", tfc2.ToString()());
         if (tfc2 != *tfcRef()) printf("Error, found TFC [%s] doesn't match original [%s]\n", tfc2.ToString()(), tfcRef()->ToString()());
      }
      else printf("Error, FindFlat() by value failed!\n");
   }

   printf("Testing adding and retrieval of FlatCountableRefs by value\n");
   {
      ByteBufferRef flatBuf = msg.FlattenToByteBuffer();
      if (flatBuf())
      {
         MessageRef newMsg = GetMessageFromPool(flatBuf);
         if (newMsg())
         {
            TestFlatCountable tfc3;
            if (newMsg()->FindFlat("tfc", tfc3).IsOK())
            {
               printf("FindFlat() found: [%s]\n", tfc3.ToString()());
               if (tfc3 != *tfcRef()) printf("Error, found TFC [%s] doesn't match original [%s]\n", tfc3.ToString()(), tfcRef()->ToString()());
            }
            else printf("Error, FindFlat() by value (from restored Message) failed!\n");
         }
         else printf("Error, couldn't restore Message from flattened buffer!\n");
      }
      else printf("ERROR, Message flatten failed!\n");
   }

   printf("\n\nFinal contents of (msg) are:\n");
   msg.PrintToStream();

   return 0;
}

