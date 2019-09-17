/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleProxyDataIO_h
#define MuscleProxyDataIO_h

#include "dataio/PacketDataIO.h"
#include "dataio/SeekableDataIO.h"

namespace muscle {
 
/** This DataIO is a "wrapper" DataIO that passes all calls through verbatim
  * to a held "child" DataIO.  It's not terribly useful by itself, but can
  * be useful as a base class for a subclass that wants to modify certain
  * DataIO calls' behaviors while leaving the rest as-is.
  */
class ProxyDataIO : public PacketDataIO, public SeekableDataIO
{
public:
   /** Default Constructor.  Be sure to set a child dataIO with SetChildDataIO()
     * before using this DataIO, so that this object will do something useful!
     */
   ProxyDataIO() : _seekableChildIO(NULL), _packetChildIO(NULL) {/* empty */}

   /** Constructor. 
     * @param childIO Reference to the DataIO to pass calls on through to
     *                after the data has been XOR'd.
     */
   ProxyDataIO(const DataIORef & childIO) {SetChildDataIO(childIO);}

   /** Destructor. */
   virtual ~ProxyDataIO() {/* empty */}

   virtual int32 Read(void * buffer, uint32 size) {return _childIO() ? _childIO()->Read(buffer, size) : -1;}
   virtual int32 Write(const void * buffer, uint32 size) {return _childIO() ? _childIO()->Write(buffer, size) : -1;}
   virtual uint64 GetOutputStallLimit() const {return _childIO() ? _childIO()->GetOutputStallLimit() : MUSCLE_TIME_NEVER;}

   virtual void FlushOutput() {if (_childIO()) _childIO()->FlushOutput();}
   virtual void Shutdown() {if (_childIO()) _childIO()->Shutdown(); _childIO.Reset();}

   virtual const ConstSocketRef & GetReadSelectSocket()  const {return _childIO() ? _childIO()->GetReadSelectSocket()  : GetNullSocket();}
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return _childIO() ? _childIO()->GetWriteSelectSocket() : GetNullSocket();}
   virtual status_t GetReadByteTimeStamp(int32 whichByte, uint64 & retStamp) const {return _childIO() ? _childIO()->GetReadByteTimeStamp(whichByte, retStamp) : B_BAD_OBJECT;}

   virtual bool HasBufferedOutput() const {return _childIO() ? _childIO()->HasBufferedOutput() : false;}
   virtual void WriteBufferedOutput() {if (_childIO()) _childIO()->WriteBufferedOutput();}

   virtual status_t Seek(int64 offset, int whence) {return _seekableChildIO ? _seekableChildIO->Seek(offset, whence) : B_BAD_OBJECT;}
   virtual int64 GetPosition() const {return _seekableChildIO ? _seekableChildIO->GetPosition() : -1;}
   virtual int64 GetLength() {return _seekableChildIO ? _seekableChildIO->GetLength() : -1;}

   virtual const IPAddressAndPort & GetSourceOfLastReadPacket() const {return _packetChildIO ? _packetChildIO->GetSourceOfLastReadPacket() : GetDefaultObjectForType<IPAddressAndPort>();}
   virtual const IPAddressAndPort & GetPacketSendDestination() const  {return _packetChildIO ? _packetChildIO->GetPacketSendDestination()  : GetDefaultObjectForType<IPAddressAndPort>();}
   virtual status_t SetPacketSendDestination(const IPAddressAndPort & iap) {return _packetChildIO ? _packetChildIO->SetPacketSendDestination(iap) : B_BAD_OBJECT;}
   virtual uint32 GetMaximumPacketSize() const {return _packetChildIO ? _packetChildIO->GetMaximumPacketSize() : 0;}
   virtual int32 ReadFrom(void * buffer, uint32 size, IPAddressAndPort & retPacketSource) {return _packetChildIO ? _packetChildIO->ReadFrom(buffer, size, retPacketSource) : -1;}
   virtual int32 WriteTo(const void * buffer, uint32 size, const IPAddressAndPort & packetDest) {return _packetChildIO ? _packetChildIO->WriteTo(buffer, size, packetDest) : -1;}

   /** Returns a reference to our held child DataIO (if any) */
   const DataIORef & GetChildDataIO() const {return _childIO;}

   /** Sets our current held child DataIO.
     * @param childDataIO The new child DataIO to forward method calls along to.
     */
   virtual void SetChildDataIO(const DataIORef & childDataIO) 
   {
      _childIO         = childDataIO;
      _seekableChildIO = dynamic_cast<SeekableDataIO *>(_childIO());
      _packetChildIO   = dynamic_cast<PacketDataIO *>(_childIO());
   }

private:
   DataIORef _childIO;
   SeekableDataIO * _seekableChildIO;  // points to the same object as (_childIO()), or NULL
   PacketDataIO   * _packetChildIO;    // points to the same object as (_childIO()), or NULL
};
DECLARE_REFTYPES(ProxyDataIO);

} // end namespace muscle

#endif
