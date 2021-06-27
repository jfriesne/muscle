/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSocket_h
#define MuscleSocket_h

#include "support/NotCopyable.h"
#include "util/RefCount.h"
#include "util/CountedObject.h"

namespace muscle {

/** Descriptor type. */
#ifdef WIN32
   typedef SOCKET SocketDescriptor;
#else
   typedef int SocketDescriptor;
#define INVALID_SOCKET -1
#endif

/** Format specifier for SocketDescriptor */
#ifdef _WIN64
#define SOCKET_FORMAT_SPEC UINT64_FORMAT_SPEC
#elif defined(_WIN32)
#define SOCKET_FORMAT_SPEC UINT32_FORMAT_SPEC
#else
#define SOCKET_FORMAT_SPEC "%i"
#endif

/** Is socket descriptor valid?
  * @param sd The socket descriptor to be tested
  */
static inline bool isValidSocket(SocketDescriptor sd) {
#ifdef WIN32
   return sd != INVALID_SOCKET;
#else
   return sd >= 0;
#endif
}

/** A simple socket-holder class to make sure that opened socket file
  * descriptors don't get accidentally leaked.  A Socket object is typically
  * handed to a ConstSocketRef, whose constructor and destructor will implement
  * the reference-counting necessary to automatically delete/recycle the Socket
  * object (and thereby automatically close its held socket descriptor) when the socket
  * descriptor is no longer needed for anything.
  */
class Socket : public RefCountable, private NotCopyable
{
public:

   /** Default constructor. */
   Socket() : _sd(INVALID_SOCKET), _okayToClose(false) {/* empty */}

   /** Constructor.
     * @param sd Descriptor of a socket.  (sd) becomes property of this Socket object.
     * @param okayToClose If true (sd) will be closed by the destructor.
     *                    If false, we will not close (sd).  Defaults to true.
     */
   explicit Socket(SocketDescriptor sd, bool okayToClose = true) : _sd(sd), _okayToClose(okayToClose) {/* empty */}

   /** Destructor.  Closes our held descriptor, if we have one. */
   virtual ~Socket();

   /** Returns and releases our held descriptor.
     * When this method returns, ownership of the socket is transferred to the calling code.
     */
   SocketDescriptor ReleaseSocketDescriptor() { SocketDescriptor ret = _sd; _sd = INVALID_SOCKET; return ret;}

   /** Returns the held socket descriptor, but does not release ownership of it. */
   SocketDescriptor GetSocketDescriptor() const {return _sd;}

   /** Sets our file descriptor.  Will close any old one if appropriate.
     * @param sd The new descriptor to hold, or INVALID_SOCKET if we shouldn't hold a descriptor any more.
     * @param okayToCloseSD true iff we should close (sd) when we're done with it, false if we shouldn't.
     *                    Note that this argument affects only what we'll do with (sd), and NOT what
     *                    we'll do with any descriptor we may already be holding!.  This argument
     *                    is not meaningful when (sd) is passed in as INVALID_SOCKET.
     */
   void SetSocketDescriptor(SocketDescriptor sd, bool okayToCloseSD = true);

   /** Resets this Socket object to its just-constructed state, freeing any held socket descriptor if appropriate.
     * This call is equivalent to calling SetFileDescriptor(INVALID_SOCKET, false).
     */
   void Clear() {SetSocketDescriptor(INVALID_SOCKET, false);}

private:
   friend class ObjectPool<Socket>;

   /** Assignment operator, used only for ObjectPool recycling, private on purpose */
   Socket & operator = (const Socket & /*rhs*/) {Clear(); return *this;}

   SocketDescriptor _sd;
   bool _okayToClose;

   DECLARE_COUNTED_OBJECT(Socket);
};

/** ConstSocketRef is subclassed rather than typedef'd so that I can override the == and != operators
  * to check for equality based on the file descriptor value rather than on the address of the
  * referenced Socket object.  Doing it this way gives more intuitive hashing behavior (i.e.
  * multiple ConstSocketRefs referencing the same file descriptor will hash to the same entry)
  */
class ConstSocketRef : public ConstRef<Socket>
{
public:
   /** Default constructor */
   ConstSocketRef() : ConstRef<Socket>() {/* empty */}

   /** This constructor takes ownership of the given Socket object.
     * @param item The Socket object to take ownership of.  This will be recycled/deleted when this ConstSocketRef is destroyed, unless (doRefCount) is specified as false.
     * @param doRefCount If set false, we will not attempt to reference-count (item), and instead will only act like a dumb pointer.  Defaults to true.
     */
   ConstSocketRef(const Socket * item, bool doRefCount = true) {SetRef(item, doRefCount);}

   /** Copy constructor
     * @param rhs The ConstSocketRef to become a copy of.  Note that this doesn't copy (rhs)'s underlying Socket object, but instead
     *            only adds one to its reference count.
     */
   ConstSocketRef(const ConstSocketRef & rhs) : ConstRef<Socket>(rhs) {/* empty */}

   /** Downcast constructor
     * @param ref A RefCountableRef object that hopefully holds a Socket object
     * @param junk This parameter doesn't mean anything; it is only here to differentiate this ctor from the other ctors.
     */
   ConstSocketRef(const RefCountableRef & ref, bool junk) : ConstRef<Socket>(ref, junk) {/* empty */}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   inline ConstSocketRef & operator = (const ConstSocketRef & rhs) {(void) ConstRef<Socket>::operator=(rhs); return *this;}

   /** Comparison operator.  Returns true iff (this) and (rhs) both contain the same file descriptor.
     * @param rhs the ConstSocketRef to compare file descriptors with
     */
   inline bool operator ==(const ConstSocketRef &rhs) const {return GetSocketDescriptor() == rhs.GetSocketDescriptor();}

   /** Comparison operator.  Returns true iff (this) and (rhs) don't both contain the same file descriptor.
     * @param rhs the ConstSocketRef to compare file descriptors with
     */
   inline bool operator !=(const ConstSocketRef &rhs) const {return GetSocketDescriptor() != rhs.GetSocketDescriptor();}

   /** Convenience method.  Returns the file descriptor we are holding, or INVALID_SOCKET if we are a NULL reference. */
   SocketDescriptor GetSocketDescriptor() const {const Socket * s = GetItemPointer(); return s?s->GetSocketDescriptor():INVALID_SOCKET;}

   /** When we're being used as a key in a Hashtable, key on the file descriptor we hold */
   uint32 HashCode() const {return CalculateHashCode(GetSocketDescriptor());}
};

/** Returns a ConstSocketRef from our ConstSocketRef pool that references the passed in file descriptor.
  * @param fd The file descriptor that the returned ConstSocketRef should be tracking.
  * @param okayToClose if true, (sd) will be closed when the last ConstSocketRef
  *                    that references it is destroyed.  If false, it won't be.
  * @param retNULLIfInvalidSocket If left true and (sd) is invalid, then a NULL ConstSocketRef
  *                               will be returned.  If set false, then we will return a
  *                               non-NULL ConstSocketRef object, with (sd)'s invalid value in it.
  * @returns a ConstSocketRef pointing to the specified socket on success, or a NULL ConstSocketRef on
  *          failure (out of memory).  Note that in the failure case, (sd) will be closed unless
  *          (okayToClose) was false; that way you don't have to worry about closing it yourself.
  */
ConstSocketRef GetConstSocketRefFromPool(SocketDescriptor sd, bool okayToClose = true, bool retNULLIfInvalidSocket = true);

/** Convenience method:  Returns a NULL socket reference. */
inline const ConstSocketRef & GetNullSocket() {return GetDefaultObjectForType<ConstSocketRef>();}

/** Convenience method:  Returns a reference to an invalid Socket (i.e. a Socket object with a negative socket descriptor).  Note the difference between what this function returns and what GetNullSocket() returns!  If you're not sure which of these two functions to use, then GetNullSocket() is probably the one you want. */
const ConstSocketRef & GetInvalidSocket();

} // end namespace muscle

#endif
