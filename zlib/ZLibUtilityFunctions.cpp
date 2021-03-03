/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING

#include "dataio/DataIO.h"
#include "message/Message.h"
#include "system/SetupSystem.h"
#include "zlib/ZLibCodec.h"
#include "zlib/ZLibUtilityFunctions.h"
#ifndef MUSCLE_AVOID_THREAD_LOCAL_STORAGE
# ifdef MUSCLE_SINGLE_THREAD_ONLY
#  define MUSCLE_AVOID_THREAD_LOCAL_STORAGE
# else
#  include "system/ThreadLocalStorage.h"
# endif
#endif

namespace muscle {

static const String MUSCLE_ZLIB_FIELD_NAME_STRING = MUSCLE_ZLIB_FIELD_NAME;

#ifdef MUSCLE_AVOID_THREAD_LOCAL_STORAGE
static Mutex _zlibLock;  // a separate lock because using the global lock was causing deadlockfinder hits
static ZLibCodec * _codecs[10] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static bool _cleanupCallbackInstalled = false;
#else
static ThreadLocalStorage<ZLibCodec> _codecs[10];  // using ThreadLocalStorage == no locking == no headaches :)
#endif

#ifdef MUSCLE_AVOID_THREAD_LOCAL_STORAGE
static void FreeZLibCodecs()
{
   if (_zlibLock.Lock().IsOK())  // probably isn't necessary since this only gets called during shutdown... but just in case
   {
      for (uint32 i=0; i<ARRAYITEMS(_codecs); i++) 
      { 
         delete _codecs[i];
         _codecs[i] = NULL;
      } 
      _zlibLock.Unlock();
   }
}

static void EnsureCleanupCallbackInstalled()
{
   if (_cleanupCallbackInstalled == false)  // check once without locking, to avoid locking every time
   {
      const Mutex * m = GetGlobalMuscleLock();
      if ((m)&&(m->Lock().IsOK()))
      {
         if (_cleanupCallbackInstalled == false)  // in case we got scooped before could aquire the lock
         {
            CompleteSetupSystem * css = CompleteSetupSystem::GetCurrentCompleteSetupSystem();
            if (css)
            {
               static FunctionCallback _freeCodecsCallback(FreeZLibCodecs);
               if (css->GetCleanupCallbacks().AddTail(GenericCallbackRef(&_freeCodecsCallback, false)).IsOK()) _cleanupCallbackInstalled = true;
            }
         }
         m->Unlock();
      }
   }
}
#endif

static ZLibCodec * GetZLibCodec(int level)
{
#ifdef MUSCLE_AVOID_THREAD_LOCAL_STORAGE
   level = muscleClamp(level, 0, 9);
   if (_codecs[level] == NULL) 
   {
      _codecs[level] = newnothrow ZLibCodec(level);  // demand-allocate
      if (_codecs[level] == NULL) MWARN_OUT_OF_MEMORY;
   }
   return _codecs[level];
#else
   return muscleInRange(level, 0, 9) ? _codecs[level].GetOrCreateThreadLocalObject() : NULL;
#endif
}

bool IsMessageDeflated(const MessageRef & msgRef) 
{
   return ((msgRef())&&(msgRef()->HasName(MUSCLE_ZLIB_FIELD_NAME_STRING)));
}

ByteBufferRef DeflateByteBuffer(const uint8 * buf, uint32 numBytes, int compressionLevel, uint32 addHeaderBytes, uint32 addFooterBytes)
{
   ByteBufferRef ret;
#ifdef MUSCLE_AVOID_THREAD_LOCAL_STORAGE
   if (_zlibLock.Lock().IsOK())  // serialize so that it's thread safe!
   {
      ZLibCodec * codec = GetZLibCodec(compressionLevel);
      if (codec) ret = codec->Deflate(buf, numBytes, true, addHeaderBytes, addFooterBytes);
      (void) _zlibLock.Unlock();
      if (codec) EnsureCleanupCallbackInstalled();  // do this outside of the ZLib lock!
   }
#else
   ZLibCodec * codec = GetZLibCodec(compressionLevel);
   if (codec) ret = codec->Deflate(buf, numBytes, true, addHeaderBytes, addFooterBytes);
#endif
   return ret;
}

ByteBufferRef InflateByteBuffer(const uint8 * buf, uint32 numBytes)
{
   ByteBufferRef ret;
#ifdef MUSCLE_AVOID_THREAD_LOCAL_STORAGE
   if (_zlibLock.Lock().IsOK())  // serialize so that it's thread safe!
   {
      ZLibCodec * codec = GetZLibCodec(6);  // doesn't matter which compression-level/codec we use, any of them can inflate anything
      if (codec) ret = codec->Inflate(buf, numBytes);
      _zlibLock.Unlock();
      if (codec) EnsureCleanupCallbackInstalled();  // do this outside of the ZLib lock!
   }
#else
   ZLibCodec * codec = GetZLibCodec(6);  // doesn't matter which compression-level/codec we use, any of them can inflate anything
   if (codec) ret = codec->Inflate(buf, numBytes);
#endif
   return ret;
}

MessageRef DeflateMessage(const MessageRef & msgRef, int compressionLevel, bool force)
{
   TCHECKPOINT;

   MessageRef ret = msgRef;
   if ((msgRef())&&(msgRef()->HasName(MUSCLE_ZLIB_FIELD_NAME_STRING) == false))
   {
      MessageRef defMsg = GetMessageFromPool(msgRef()->what);
      ByteBufferRef buf = msgRef()->FlattenToByteBuffer();
      if ((defMsg())&&(buf()))
      {
         buf = DeflateByteBuffer(*buf(), compressionLevel);
         if ((buf())&&(defMsg()->AddFlat(MUSCLE_ZLIB_FIELD_NAME_STRING, buf).IsOK())&&((force)||(defMsg()->FlattenedSize() < msgRef()->FlattenedSize()))) ret = defMsg;
      }
   }
   return ret;
}

MessageRef InflateMessage(const MessageRef & msgRef)
{
   TCHECKPOINT;

   MessageRef ret;
   ByteBufferRef bufRef;
   if ((msgRef())&&(msgRef()->FindFlat(MUSCLE_ZLIB_FIELD_NAME_STRING, bufRef).IsOK()))
   {
      MessageRef infMsg = GetMessageFromPool();
      if (infMsg())
      {
         bufRef = InflateByteBuffer(*bufRef());
         if ((bufRef())&&(infMsg()->UnflattenFromByteBuffer(bufRef).IsOK())) 
         {
            infMsg()->what = msgRef()->what;  // do this after UnflattenFromByteBuffer(), so that the outer 'what' is the one that gets used
            ret = infMsg;
         }
      }
   }
   else ret = msgRef;

   return ret;
}

status_t ReadAndDeflateAndWrite(DataIO & sourceRawIO, DataIO & destDeflatedIO, bool independent, uint32 numBytesToRead, int compressionLevel)
{
#ifdef MUSCLE_AVOID_THREAD_LOCAL_STORAGE
   ZLibCodec codec(compressionLevel);   // no sense dealing with global locks on shared codecs, since this operation is likely to be slow anyway
   return codec.ReadAndDeflateAndWrite(sourceRawIO, destDeflatedIO, independent, numBytesToRead);
#else
   ZLibCodec * codec = GetZLibCodec(compressionLevel);
   return codec ? codec->ReadAndDeflateAndWrite(sourceRawIO, destDeflatedIO, independent, numBytesToRead) : B_BAD_ARGUMENT;
#endif
}

status_t ReadAndInflateAndWrite(DataIO & sourceDeflatedIO, DataIO & destInflatedIO)
{
#ifdef MUSCLE_AVOID_THREAD_LOCAL_STORAGE
   ZLibCodec codec;   // no sense dealing with global locks on shared codecs, since this operation is likely to be slow anyway
   return codec.ReadAndInflateAndWrite(sourceDeflatedIO, destInflatedIO);
#else
   ZLibCodec * codec = GetZLibCodec(6);  // doesn't matter which one we use, any of them can inflate anything
   return codec ? codec->ReadAndInflateAndWrite(sourceDeflatedIO, destInflatedIO) : B_BAD_ARGUMENT;
#endif
}

} // end namespace muscle

#endif
