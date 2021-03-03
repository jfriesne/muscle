/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "dataio/FileDataIO.h"
#include "system/SetupSystem.h"
#include "message/Message.h"
#include "util/ByteBuffer.h"
#include "util/Hashtable.h"
#include "zlib/ZLibUtilityFunctions.h"

using namespace muscle;

static String GetBytesSizeString(uint64 val)
{
   const double b = 1000.0;  // note that we defined 1KB=1000 bytes, not 1024 bytes!
        if (val < b)     return String("%1 bytes").Arg(val);
   else if (val < b*b)   return String("%1kB").Arg(((double)val)/b,       "%.0f");
   else if (val < b*b*b) return String("%1MB").Arg(((double)val)/(b*b),   "%.01f");
   else                  return String("%1GB").Arg(((double)val)/(b*b*b), "%.02f");
}

static void GenerateMessageSizeReportAux(const String & curPath, Message & msg, Hashtable<String, uint32> & results)
{
   for (MessageFieldNameIterator fnIter(msg, B_MESSAGE_TYPE); fnIter.HasData(); fnIter++)
   {
      const String & fn = fnIter.GetFieldName();
      MessageRef subMsg;
      for (uint32 i=0; msg.FindMessage(fn, i, subMsg).IsOK(); i++)
      {
         String subPath = curPath + "/" + fn;
         if (i > 0) subPath += String(":%1").Arg(i+1);
         GenerateMessageSizeReportAux(subPath, *subMsg(), results);
      }
      (void) msg.RemoveName(fn);  // so that child-fields won't be counted in our own payload-size below
   }
   (void) results.Put(curPath, msg.FlattenedSize());
}

static void PrintMessageReport(Message & msg, bool isSizeReport)
{
   if (isSizeReport)
   {
      Hashtable<String, uint32> results;
      GenerateMessageSizeReportAux(GetEmptyString(), msg, results);
      results.SortByValue();
      for (HashtableIterator<String, uint32> iter(results); iter.HasData(); iter++) printf("%s:  %s\n", GetBytesSizeString(iter.GetValue())(), iter.GetKey()());
   }
   else msg.PrintToStream();
}

// A simple utility to read in a flattened Message file from disk, and print it out.
int main(int argc, char ** argv)
{
   int retVal = 0;

   CompleteSetupSystem css;

   const char * fileName   = (argc > 1) ? argv[1] : "test.msg";
   const bool isSizeReport = (((argc > 2)&&(strstr(argv[2], "sizes") != NULL))||(String(argv[0]).EndsWith("sizes")));
   FILE * fpIn = muscleFopen(fileName, "rb");
   if (fpIn)
   {
      FileDataIO fdio(fpIn);

      const uint64 fileSize = fdio.GetLength();
      printf("fileSize=" UINT64_FORMAT_SPEC "\n", fileSize);

      ByteBufferRef buf = GetByteBufferFromPool(fileSize);
      if (buf() == NULL)
      {
         MWARN_OUT_OF_MEMORY;
         return 10;
      }

      const uint32 numBytesRead = fdio.ReadFully(buf()->GetBuffer(), buf()->GetNumBytes());
      if (numBytesRead == fileSize) LogTime(MUSCLE_LOG_INFO, "Read " INT32_FORMAT_SPEC " bytes from [%s]\n", numBytesRead, fileName);
      else
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Short read error (" UINT32_FORMAT_SPEC "/" UINT64_FORMAT_SPEC " bytes read)\n", numBytesRead, fileSize);
         return 10;
      }

      ByteBufferRef infBuf = InflateByteBuffer(buf);
      if (infBuf())
      {
         LogTime(MUSCLE_LOG_INFO, "Zlib-inflated file data from " INT32_FORMAT_SPEC " to " UINT32_FORMAT_SPEC " bytes.\n", numBytesRead, infBuf()->GetNumBytes());
         buf = infBuf;
      }

      status_t ret;
      Message msg;
      if (msg.Unflatten(buf()->GetBuffer(), buf()->GetNumBytes()).IsOK(ret))
      {
         MessageRef infMsg = InflateMessage(MessageRef(&msg, false));
         if ((infMsg())&&(infMsg() != &msg))
         {
            LogTime(MUSCLE_LOG_INFO, "Zlib-inflated Message from " UINT32_FORMAT_SPEC " bytes to " UINT32_FORMAT_SPEC " bytes\n", msg.FlattenedSize(), infMsg()->FlattenedSize());
            LogTime(MUSCLE_LOG_INFO, "Message is:\n");
            PrintMessageReport(*infMsg(), isSizeReport);
            infMsg()->PrintToStream();
         }
         else
         {
            LogTime(MUSCLE_LOG_INFO, "Message is:\n");
            PrintMessageReport(msg, isSizeReport);
         }
      }
      else 
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Error [%s] unflattening message! (" INT32_FORMAT_SPEC " bytes read)\n", ret(), numBytesRead);
         retVal = 10;
      }
   }
   else 
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Could not read input flattened-message file [%s] [%s]\n", fileName, B_ERRNO());
      retVal = 10;
   }

   return retVal;
}
