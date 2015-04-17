/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleFailoverDataIO_h
#define MuscleFailoverDataIO_h

#include "dataio/DataIO.h"
#include "util/Queue.h"

namespace muscle {

class FailoverDataIO;

/** This class represents any object that can receive a callback notification that a failover has occurred. */
class IFailoverNotifyTarget
{
public:
   /** Default ctor */
   IFailoverNotifyTarget() {/* empty */}

   /** Destructor */
   virtual ~IFailoverNotifyTarget() {/* empty */}

   /** Called by a FailoverDataIO object when a failover occurs.
     * @param source The calling FailoverDataIO object.
     */
   virtual void DataIOFailover(const FailoverDataIO & source) = 0;
};

/** This DataIO holds a list of one or more other DataIO objects, and uses the only
  * first one until an error occurs.  If an error occurs, it will discard the first
  * held DataIO and start using the second one instead (and so on).  This is useful
  * for providing automatic failover/redundancy for important connections.
  */
class FailoverDataIO : public DataIO, private CountedObject<FailoverDataIO>
{
public:
   /** Default Constructor.  Be sure to add some child DataIOs to our Queue of
     * DataIOs (as returned by GetChildDataIOs()) so that this object will do something useful!
     * @param logErrorLevel Error level to give the log message that is generated when a failover occurs.
     *                      Defaults to MUSCLE_LOG_NONE, so that by default no log message is generated.
     */
   FailoverDataIO(int logErrorLevel = MUSCLE_LOG_NONE) : _logErrorLevel(logErrorLevel), _target(NULL) {/* empty */}

   /** Virtual destructor, to keep C++ honest.  */
   virtual ~FailoverDataIO() {/* empty */}

   virtual int32 Read(void * buffer, uint32 size)
   {
      while(HasChild())
      {
         int32 ret = GetChild()->Read(buffer, size);
         if (ret >= 0) return ret;
                  else Failover();
      }
      return -1;
   }

   virtual int32 Write(const void * buffer, uint32 size)
   {
      while(HasChild())
      {
         int32 ret = GetChild()->Write(buffer, size);
         if (ret >= 0) return ret;
                  else Failover();
      }
      return -1;
   }

   virtual status_t Seek(int64 offset, int whence)
   {
      while(HasChild())
      {
         status_t ret = GetChild()->Seek(offset, whence);
         if (ret == B_NO_ERROR) return ret;
                           else Failover();
      }
      return B_ERROR;
   }

   virtual int64 GetPosition() const {return HasChild() ? GetChild()->GetPosition() : -1;}

   virtual uint64 GetOutputStallLimit() const {return HasChild() ? GetChild()->GetOutputStallLimit() : MUSCLE_TIME_NEVER;}

   virtual void FlushOutput() {if (HasChild()) GetChild()->FlushOutput();}

   virtual void Shutdown() {_childIOs.Clear();}

   virtual const ConstSocketRef & GetReadSelectSocket()  const {return HasChild() ? GetChild()->GetReadSelectSocket()  : GetNullSocket();}
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return HasChild() ? GetChild()->GetWriteSelectSocket() : GetNullSocket();}

   virtual status_t GetReadByteTimeStamp(int32 whichByte, uint64 & retStamp) const {return HasChild() ? GetChild()->GetReadByteTimeStamp(whichByte, retStamp) : B_ERROR;}

   virtual bool HasBufferedOutput() const {return HasChild() ? GetChild()->HasBufferedOutput() : false;}

   virtual void WriteBufferedOutput() {if (HasChild()) GetChild()->WriteBufferedOutput();}

   virtual uint32 GetPacketMaximumSize() const {return (HasChild()) ? GetChild()->GetPacketMaximumSize() : 0:}

   /** Returns a read-only reference to our list of child DataIO objects. */
   const Queue<DataIORef> & GetChildDataIOs() const {return _childIOs;}

   /** Returns a read/write reference to our list of child DataIO objects. */
   Queue<DataIORef> & GetChildDataIOs() {return _childIOs;}

   /** Called whenever a child DataIO reports an error.  Default implementation
     * removes the first DataIORef from the queue, prints an error to the log,
     * and calls DataIOFailover() on the current failover notification target
     * (if any; see SetFailoverNotifyTarget() for details)
     */
   virtual void Failover()
   {
      (void) _childIOs.RemoveHead();
      if (HasChild()) LogTime(_logErrorLevel, "FailoverDataIO:  Child IO errored out, failing over to next child ("UINT32_FORMAT_SPEC" children left)!\n", _childIOs.GetNumItems());
                 else LogTime(_logErrorLevel, "FailoverDataIO:  Child IO errored out, no backup children left!\n");
      if (_target) _target->DataIOFailover(*this);
   }

   /** Call this to set the object that we should call DataIOFailover() on when a failover occurs. */
   void SetFailoverNotifyTarget(IFailoverNotifyTarget * t) {_target = t;}

   /** Returns the current failover notification target.  Default is NULL (i.e. no target set) */
   IFailoverNotifyTarget * GetFailoverNotifyTarget() const {return _target;}

private:
   bool HasChild() const {return (_childIOs.HasItems());}
   DataIO * GetChild() const {return (_childIOs.Head()());}

   Queue<DataIORef> _childIOs;
   int _logErrorLevel;
   IFailoverNotifyTarget * _target;
};

}; // end namespace muscle

#endif
