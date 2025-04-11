/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

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
   DECLARE_MUTEXGUARD(_zlibLock);  // probably isn't necessary since this only gets called during shutdown... but just in case
   for (uint32 i=0; i<ARRAYITEMS(_codecs); i++)
   {
      delete _codecs[i];
      _codecs[i] = NULL;
   }
}

static void EnsureCleanupCallbackInstalled()
{
   if (_cleanupCallbackInstalled == false)  // check once without locking, to avoid locking every time
   {
      const Mutex * m = GetGlobalMuscleLock();
      if (m)
      {
         DECLARE_MUTEXGUARD(*m);
         if (_cleanupCallbackInstalled == false)  // in case we got scooped before could aquire the lock
         {
            CompleteSetupSystem * css = CompleteSetupSystem::GetCurrentCompleteSetupSystem();
            if (css)
            {
               static FunctionCallback _freeCodecsCallback(FreeZLibCodecs);
               if (css->GetCleanupCallbacks().AddTail(DummyGenericCallbackRef(_freeCodecsCallback)).IsOK()) _cleanupCallbackInstalled = true;
            }
         }
      }
   }
}
#endif

static ZLibCodec * GetZLibCodec(int level)
{
#ifdef MUSCLE_AVOID_THREAD_LOCAL_STORAGE
   level = muscleClamp(level, 0, 9);
   if (_codecs[level] == NULL) _codecs[level] = new ZLibCodec(level);  // demand-allocate
   return _codecs[level];
#else
   return muscleInRange(level, 0, 9) ? _codecs[level].GetOrCreateThreadLocalObject() : NULL;
#endif
}

bool IsMessageDeflated(const ConstMessageRef & msgRef)
{
   return ((msgRef())&&(msgRef()->HasName(MUSCLE_ZLIB_FIELD_NAME_STRING)));
}

ByteBufferRef DeflateByteBuffer(const uint8 * buf, uint32 numBytes, int compressionLevel, uint32 addHeaderBytes, uint32 addFooterBytes)
{
   ByteBufferRef ret;
#ifdef MUSCLE_AVOID_THREAD_LOCAL_STORAGE
   DECLARE_NAMED_MUTEXGUARD(mg, _zlibLock);
   ZLibCodec * codec = GetZLibCodec(compressionLevel);
   if (codec) ret = codec->Deflate(buf, numBytes, true, addHeaderBytes, addFooterBytes);
   mg.UnlockEarly();  // because we want the line below to execute the outside of the critical section
   if (codec) EnsureCleanupCallbackInstalled();
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
   DECLARE_NAMED_MUTEXGUARD(mg, _zlibLock);
   ZLibCodec * codec = GetZLibCodec(6);  // doesn't matter which compression-level/codec we use, any of them can inflate anything
   if (codec) ret = codec->Inflate(buf, numBytes);
   mg.UnlockEarly();  // because we want the line below to execute the outside of the critical section
   if (codec) EnsureCleanupCallbackInstalled();
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
         if ((buf())&&(defMsg()->AddFlat(MUSCLE_ZLIB_FIELD_NAME_STRING, buf).IsOK())&&((force)||(defMsg()->FlattenedSize() < msgRef()->FlattenedSize()))) ret = std_move_if_available(defMsg);
      }
   }
   return ret;
}

MessageRef InflateMessage(const MessageRef & msgRef)
{
   TCHECKPOINT;

   if (msgRef() == NULL) return msgRef;

   MessageRef ret;
   ConstByteBufferRef bufRef = msgRef()->GetFlat<ConstByteBufferRef>(MUSCLE_ZLIB_FIELD_NAME_STRING);
   if (bufRef())
   {
      MessageRef infMsg = GetMessageFromPool();
      if (infMsg())
      {
         bufRef = InflateByteBuffer(*bufRef());
         if ((bufRef())&&(infMsg()->UnflattenFromByteBuffer(bufRef).IsOK()))
         {
            infMsg()->what = msgRef()->what;  // do this after UnflattenFromByteBuffer(), so that the outer 'what' is the one that gets used
            ret = std_move_if_available(infMsg);
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
