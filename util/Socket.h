/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSocket_h
#define MuscleSocket_h

#include "support/NotCopyable.h"
#include "util/RefCount.h"
#include "util/CountedObject.h"

namespace muscle {

/** A simple socket-holder class to make sure that opened socket file
  * descriptors don't get accidentally leaked.  A Socket object is typically
  * handed to a ConstSocketRef, whose constructor and destructor will implement
  * the reference-counting necessary to automatically delete/recycle the Socket
  * object (and thereby automatically close its held file-descriptor) when the file
  * descriptor is no longer needed for anything.
  */
class Socket : public RefCountable, private NotCopyable
{
public:
   /** Default constructor. */
   Socket() : _family(SOCKET_FAMILY_INVALID), _fd(-1), _okayToClose(false) {/* empty */}

   /** Constructor.
     * @param fd File descriptor of a socket.  If (okayToClose) is true, then (fd) becomes property of this Socket object.
     * @param okayToClose If true (fd) will be closed by the destructor.
     *                    If false, we will not close (fd).  Defaults to true.
     */
   explicit Socket(int fd, bool okayToClose = true) : _family(Socket::GetFamilyForFD(fd)), _fd(fd), _okayToClose(okayToClose) {/* empty */}

#ifdef WIN32
   /** Convenience-constructor, for Windows only.  Accepts a SOCKET as an argument instead of an int.
     * @param s Windows SOCKET descriptor.  If (okayToClose) is true, then (s) becomes property of this Socket object.
     * @param okayToClose If true (s) will be closed by the destructor.
     *                    If false, we will not close (s).  Defaults to true.
     */
   explicit Socket(SOCKET s, bool okayToClose = true) : _family(Socket::GetFamilyForFD(static_cast<int>(s))), _fd(static_cast<int>(s)), _okayToClose(okayToClose) {/* empty */}
#endif

   /** Destructor.  Closes our held file-descriptor, if we have one. */
   virtual ~Socket();

   /** Returns a SOCKET_FAMILY_* value indicating what sort of socket this is */
   int GetFamily() const {return _family;}

   /** Returns and releases our held file-descriptor.
     * When this method returns, ownership of the socket is transferred to the calling code.
     */
   int ReleaseFileDescriptor() {int ret = _fd; _family = SOCKET_FAMILY_INVALID; _fd = -1; return ret;}

   /** Returns the held socket fd, but does not release ownership of it. */
   int GetFileDescriptor() const {return _fd;}

   /** Sets our file-descriptor.  Will close any old one if appropriate.
     * @param fd The new file-descriptor to hold, or -1 if we shouldn't hold a file-descriptor any more.
     * @param okayToCloseFD true iff we should close (fd) when we're done with it, false if we shouldn't.
     *                    Note that this argument affects only what we'll do with (fd), and NOT what
     *                    we'll do with any file-descriptor we may already be holding!.  This argument
     *                    is not meaningful when (fd) is passed in as -1.
     */
   void SetFileDescriptor(int fd, bool okayToCloseFD = true);

   /** Resets this Socket object to its just-constructed state, freeing any held socket descriptor if appropriate.
     * This call is equivalent to calling SetFileDescriptor(-1, false).
     */
   void Clear() {SetFileDescriptor(-1, false);}

   /** Returns a SOCKET_FAMILY_* value describing the type of socket we are holding.
     * Returns SOCKET_FAMILY_INVALID if we are not holding any file descriptor at all.
     */
   int GetSocketFamily() const {return _family;}

private:
   friend class ObjectPool<Socket>;

   static int GetFamilyForFD(int fd);

   /** Assignment operator, used only for ObjectPool recycling, private on purpose */
   Socket & operator = (const Socket & /*rhs*/) {Clear(); return *this;}

   int _family;
   int _fd;
   bool _okayToClose;

   DECLARE_COUNTED_OBJECT(Socket);
};

class DummyConstSocketRef;  // forward declaration

/** ConstSocketRef is subclassed rather than typedef'd so that I can override the == and != operators
  * to check for equality based on the file-descriptor value rather than on the address of the
  * referenced Socket object.  Doing it this way gives more intuitive hashing behavior (ie
  * multiple ConstSocketRefs referencing the same file-descriptor will hash to the same entry)
  */
class ConstSocketRef : public ConstRef<Socket>
{
public:
   /** Default constructor */
   ConstSocketRef() : ConstRef<Socket>() {/* empty */}

   /** This constructor takes ownership of the given Socket object.
     * Once referenced, (sock) will be automatically deleted (or recycled) and its file-descriptor closed when the last ConstSocketRef that references it goes away.
     * @param sock A dynamically allocated Socket object that the ConstSocketRef class will assume responsibility for deleting.  May be NULL.
     */
   explicit ConstSocketRef(const Socket * sock) : ConstRef<Socket>(sock) {/* empty */}

   /** Copy constructor
     * @param rhs The ConstSocketRef to become a copy of.  Note that this doesn't copy (rhs)'s underlying Socket object, but instead
     *            only adds one to its reference count.
     */
   ConstSocketRef(const ConstSocketRef & rhs) : ConstRef<Socket>(rhs) {/* empty */}

   /** Error-constructor.  Sets this ConstSocketRef to be a NULL reference with the specified error code.
     * @param status the B_* error-code to contain.  If you pass in B_NO_ERROR, then B_NULL_REF will be used by default.
     */
   ConstSocketRef(status_t status) : ConstRef<Socket>(status) {/* empty */}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   inline ConstSocketRef & operator = (const ConstSocketRef & rhs) {(void) ConstRef<Socket>::operator=(rhs); return *this;}

   /** Comparison operator.  Returns true iff (this) and (rhs) both contain the same file-descriptor.
     * @param rhs the ConstSocketRef to compare file-descriptors with
     */
   inline bool operator ==(const ConstSocketRef &rhs) const {return GetFileDescriptor() == rhs.GetFileDescriptor();}

   /** Comparison operator.  Returns true iff (this) and (rhs) don't both contain the same file-descriptor.
     * @param rhs the ConstSocketRef to compare file-descriptors with
     */
   inline bool operator !=(const ConstSocketRef &rhs) const {return GetFileDescriptor() != rhs.GetFileDescriptor();}

   /** Convenience method.  Returns the file-descriptor we are holding, or -1 if we are a NULL reference. */
   int GetFileDescriptor() const {const Socket * s = GetItemPointer(); return s?s->GetFileDescriptor():-1;}

   /** When we're being used as a key in a Hashtable, key on the file-descriptor we hold */
   uint32 HashCode() const {return CalculateHashCode(GetFileDescriptor());}

   /** Convenience method.  Returns the SOCKET_FAMILY_* value of the Socket we are holding, or SOCKET_FAMILY_INVALID if we are a NULL reference. */
   int GetSocketFamily() const {const Socket * s = GetItemPointer(); return s?s->GetSocketFamily():SOCKET_FAMILY_INVALID;}

private:
   ConstSocketRef(const Socket * sock, bool doRefCount) {SetRef(sock, doRefCount);}

   friend class DummyConstSocketRef;
};

/** This class is just syntactic sugar for more clearly declaring a
  * ConstSocketRef that doesn't actually do any reference-counting of the Socket object
  * that it refers to (eg if you need a ConstSocketRef to a stack-based Socket and
  * are willing to take responsibility for manually managing object-lifetime issues
  * yourself).  It will behave similarly to a raw pointer.
  */
class DummyConstSocketRef : public ConstSocketRef
{
public:
   /** Default constructor.  Creates a NULL reference. */
   DummyConstSocketRef() {/* empty */}

   /** Creates a dummy-const-reference to the specified Socket object
     * @param sock reference to the Socket to point to
     */
   explicit DummyConstSocketRef(const Socket & sock) : ConstSocketRef(&sock, false) {/* empty */}

   /** Creates a dummy-const-reference to the specified socket
     * @param sock pointer to the Socket to point to, or NULL to create a NULL DummyConstSocketRef.
     */
   explicit DummyConstSocketRef(const Socket * sock) : ConstSocketRef(sock, false) {/* empty */}
};

/** Returns a ConstSocketRef from our ConstSocketRef pool that references the passed-in file-descriptor.
  * @param fd The file-descriptor that the returned ConstSocketRef should be tracking.
  * @param okayToClose if true, (fd) will be closed when the last ConstSocketRef
  *                    that references it is destroyed.  If false, it won't be.  Defaults to true.
  * @param retNULLIfInvalidSocket If left true and (fd) is negative, then a NULL ConstSocketRef
  *                               will be returned.  If set false, then we will return a
  *                               non-NULL ConstSocketRef object, with (fd)'s negative value in it.
  * @returns a ConstSocketRef pointing to the specified socket on success, or a NULL ConstSocketRef on
  *          failure (out of memory).  Note that in the failure case, (fd) will be closed unless
  *          (okayToClose) was false; that way you don't have to worry about closing it yourself.
  */
ConstSocketRef GetConstSocketRefFromPool(int fd, bool okayToClose = true, bool retNULLIfInvalidSocket = true);

#ifdef WIN32
/** Returns a ConstSocketRef from our ConstSocketRef pool that references the passed-in socket-descriptor.
  * @param s The Windows SOCKET that the returned ConstSocketRef should be tracking.
  * @param okayToClose if true, (s) will be closed when the last ConstSocketRef
  *                    that references it is destroyed.  If false, it won't be.  Defaults to true.
  * @param retNULLIfInvalidSocket If left true and (s) is invalid, then a NULL ConstSocketRef
  *                               will be returned.  If set false, then we will return a
  *                               non-NULL ConstSocketRef object, with (s)'s negative value in it.
  * @returns a ConstSocketRef pointing to the specified socket on success, or a NULL ConstSocketRef on
  *          failure (out of memory).  Note that in the failure case, (s) will be closed unless
  *          (okayToClose) was false; that way you don't have to worry about closing it yourself.
  */
static inline ConstSocketRef GetConstSocketRefFromPool(SOCKET s, bool okayToClose = true, bool retNULLIfInvalidSocket = true) {return GetConstSocketRefFromPool(static_cast<int>(s), okayToClose, retNULLIfInvalidSocket);}
#endif

/** Convenience method:  Returns a NULL socket reference. */
inline const ConstSocketRef & GetNullSocket() {return GetDefaultObjectForType<ConstSocketRef>();}

/** Convenience method:  Returns a reference to an invalid Socket (ie a Socket object with a negative file-descriptor).  Note the difference between what this function returns and what GetNullSocket() returns!  If you're not sure which of these two functions to use, then GetNullSocket() is probably the one you want. */
const ConstSocketRef & GetInvalidSocket();

} // end namespace muscle

#endif
