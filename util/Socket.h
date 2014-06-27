/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSocket_h
#define MuscleSocket_h

#include "util/RefCount.h"
#include "util/CountedObject.h"

namespace muscle {

/** A simple socket-holder class to make sure that opened socket file
  * descriptors don't get accidentally leaked.  A Socket object is typically
  * handed to a ConstSocketRef, whose constructor and destructor will implement
  * the reference-counting necessary to automatically delete/recycle the Socket
  * object (and thereby automatically close its held file descriptor) when the file 
  * descriptor is no longer needed for anything.
  */
class Socket : public RefCountable, public CountedObject<Socket>
{
public:
   /** Default constructor. */
   Socket() : _fd(-1), _okayToClose(false) {/* empty */}

   /** Constructor.
     * @param fd File descriptor of a socket.  (fd) becomes property of this Socket object.
     * @param okayToClose If true (fd) will be closed by the destructor.
     *                    If false, we will not close (fd).  Defaults to true. 
     */
   explicit Socket(int fd, bool okayToClose = true) : _fd(fd), _okayToClose(okayToClose) {/* empty */}

   /** Destructor.  Closes our held file descriptor, if we have one. */
   virtual ~Socket();

   /** Returns and releases our held file descriptor.   
     * When this method returns, ownership of the socket is transferred to the calling code.
     */
   int ReleaseFileDescriptor() {int ret = _fd; _fd = -1; return ret;}

   /** Returns the held socket fd, but does not release ownership of it. */  
   int GetFileDescriptor() const {return _fd;}

   /** Sets our file descriptor.  Will close any old one if appropriate.
     * @param fd The new file descriptor to hold, or -1 if we shouldn't hold a file descriptor any more.
     * @param okayToCloseFD true iff we should close (fd) when we're done with it, false if we shouldn't.
     *                    Note that this argument affects only what we'll do with (fd), and NOT what
     *                    we'll do with any file descriptor we may already be holding!.  This argument
     *                    is not meaningful when (fd) is passed in as -1.
     */
   void SetFileDescriptor(int fd, bool okayToCloseFD = true);

   /** Resets this Socket object to its just-constructed state, freeing any held socket descriptor if appropriate. 
     * This call is equivalent to calling SetFileDescriptor(-1, false).
     */ 
   void Clear() {SetFileDescriptor(-1, false);}

private:
   friend class ObjectPool<Socket>;

   /** Copy constructor, private and unimplemented on purpose */
   Socket(const Socket &);

   /** Assignment operator, used only for ObjectPool recycling, private on purpose */
   Socket & operator = (const Socket & /*rhs*/) {Clear(); return *this;}

   int _fd;
   bool _okayToClose;
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
   ConstSocketRef(const Socket * item, bool doRefCount = true) : ConstRef<Socket>(item, doRefCount) {/* empty */}

   /** Copy constructor
     * @param copyMe The ConstSocketRef to become a copy of.  Note that this doesn't copy (copyMe)'s underlying Socket object, but instead
     *               only adds one to its reference count.
     */
   ConstSocketRef(const ConstSocketRef & copyMe) : ConstRef<Socket>(copyMe) {/* empty */}

   /** Downcast constructor
     * @param ref A RefCountableRef object that hopefully holds a Socket object
     * @param junk This parameter doesn't mean anything; it is only here to differentiate this ctor from the other ctors.
     */
   ConstSocketRef(const RefCountableRef & ref, bool junk) : ConstRef<Socket>(ref, junk) {/* empty */}

   /** Comparison operator.  Returns true iff (this) and (rhs) both contain the same file descriptor. */
   inline bool operator ==(const ConstSocketRef &rhs) const {return GetFileDescriptor() == rhs.GetFileDescriptor();}

   /** Comparison operator.  Returns false iff (this) and (rhs) both contain the same file descriptor. */
   inline bool operator !=(const ConstSocketRef &rhs) const {return GetFileDescriptor() != rhs.GetFileDescriptor();}

   /** Convenience method.  Returns the file descriptor we are holding, or -1 if we are a NULL reference. */
   int GetFileDescriptor() const {const Socket * s = GetItemPointer(); return s?s->GetFileDescriptor():-1;}

   /** When we're being used as a key in a Hashtable, key on the file descriptor we hold */
   uint32 HashCode() const {return CalculateHashCode(GetFileDescriptor());}
};

/** Returns a ConstSocketRef from our ConstSocketRef pool that references the passed in file descriptor.
  * @param fd The file descriptor that the returned ConstSocketRef should be tracking.
  * @param okayToClose if true, (fd) will be closed when the last ConstSocketRef 
  *                    that references it is destroyed.  If false, it won't be.
  * @param retNULLIfInvalidSocket If left true and (fd) is negative, then a NULL ConstSocketRef
  *                               will be returned.  If set false, then we will return a
  *                               non-NULL ConstSocketRef object, with (fd)'s negative value in it.
  * @returns a ConstSocketRef pointing to the specified socket on success, or a NULL ConstSocketRef on
  *          failure (out of memory).  Note that in the failure case, (fd) will be closed unless
  *          (okayToClose) was false; that way you don't have to worry about closing it yourself.
  */
ConstSocketRef GetConstSocketRefFromPool(int fd, bool okayToClose = true, bool retNULLIfInvalidSocket = true);

/** Convenience method:  Returns a NULL socket reference. */
inline const ConstSocketRef & GetNullSocket() {return GetDefaultObjectForType<ConstSocketRef>();}

/** Convenience method:  Returns a reference to an invalid Socket (i.e. a Socket object with a negative file descriptor).  Note the difference between what this function returns and what GetNullSocket() returns!  If you're not sure which of these two functions to use, then GetNullSocket() is probably the one you want. */
const ConstSocketRef & GetInvalidSocket();

}; // end namespace muscle

#endif
