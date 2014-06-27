/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef AbstractSessionIOPolicy_h
#define AbstractSessionIOPolicy_h

#include "util/RefCount.h"
#include "util/PulseNode.h"

namespace muscle {

class AbstractReflectSession;
class ReflectServer;

/** A simple data-holding class; holds an AbstractSessionIOPolicy and a boolean to
  * indicate whether it is using us as an input or not.  Keeping the two
  * values together here lets us use them as a "key".
  */
class PolicyHolder
{
public:
   /** Default Constructor.  Sets session to NULL and asInput to false */
   PolicyHolder() : _session(NULL), _asInput(false) {/* empty */}

   /** Constructor.  Sets our held values as specified by the arguments. */
   PolicyHolder(AbstractReflectSession * session, bool asInput) : _session(session), _asInput(asInput) {/* empty */}

   /** Accessor */
   AbstractReflectSession * GetSession() const {return _session;}

   /** Accessor */
   bool IsAsInput() const {return _asInput;}

   /** Returns a decent hash code for this object */
   uint32 HashCode() const {return ((uint32)((uintptr)_session))+(_asInput?1:0);}  // double-cast for AMD64

   /** Equality operator;  returns true iff (rhs) has the same two settings as we do */
   bool operator == (const PolicyHolder & rhs) {return ((rhs._session == _session)&&(rhs._asInput == _asInput));}

private:
   AbstractReflectSession * _session;
   bool _asInput;
};

/**
 * This class is an interface for objects that can be used by a ReflectServer
 * to control how and when I/O in a particular direction is performed for a
 * session or group of sessions.  Its primary use is to implement aggregate
 * bandwidth limits for certain sessions or groups of sessions.  Each
 * AbstractReflectSession can associate itself an IO policy for reading,
 * and an IO policy for writing, if it likes.
 * <p>
 * ReflectServer calls the methods in this API in the following sequence:
 * <ol>
 *  <li>BeginIO() called</li>
 *  <li>OkayToTransfer() (called once for each session that wants to transfer data)</li>
 *  <li>GetMaxTransferChunkSize() (called once for each session that was given the okay in the previous step())</li>
 *  <li>server blocks until an event is ready, and then calls DoInput() and/or DoOutput() on sessions that need it()</li>
 *  <li>EndIO() called()</li>
 *  <li>Lather, rinse, repeat</li>
 * </ol>
 */
class AbstractSessionIOPolicy : public PulseNode, public RefCountable, private CountedObject<AbstractSessionIOPolicy>
{
public:
   /** Default constructor. */
   AbstractSessionIOPolicy() : _hasBegun(false) {/* empty */}

   /** Destructor. */
   virtual ~AbstractSessionIOPolicy() {/* empty */}

   /** Called whenever an AbstractReflectSession chooses us as one of his policies of choice.
     * Guaranteed not be called between calls to BeginIO() and EndIO().
     * @param holder An object containing info on the AbstractReflectSession that
     *               is now using this policy.
     */
   virtual void PolicyHolderAdded(const PolicyHolder & holder) = 0;

   /** Called whenever an AbstractReflectSession cancels one of his policies with us.
     * Guaranteed not be called between calls to BeginIO() and EndIO().
     * @param holder An object containing info on the AbstractReflectSession that
     *               is nolonger using this policy.  Note that the contained
     *               session may be in the process of being destroyed, so while
     *               it is still a valid AbstractReflectSession, it may no longer
     *               be a valid object of its original subclass.
     */
   virtual void PolicyHolderRemoved(const PolicyHolder & holder) = 0;

   /** Called once at the beginning of each pass through our I/O event loop.
     * @param now The time at which the loop is beginning, for convenience.
     */
   virtual void BeginIO(uint64 now) = 0;

   /** Should return true iff we want to allow the given session to be able to transfer
     * bytes to/from its DataIO object.
     * Called after BeginIO() for each iteration. Only called for the sessions
     * that want to transfer data (i.e. whose gateway objects returned true from their
     * IsReadyForInput() or HasBytesToOutput() methods), so you can determine the set
     * of 'active' sessions based on what gets called here.
     * @param holder a session who wishes to transfer data.  Guaranteed to be one of
     *               our current PolicyHolders, and attached to the server.
     * @returns true iff we want to let the session transfer, false if not.
     *               (Returning false keeps the session's socket from being included
     *                in the select() call, so the server won't be awoken even if the
     *                session's socket becomes ready)
     */
   virtual bool OkayToTransfer(const PolicyHolder & holder) = 0;

   /** Should return the maximum number of bytes that the given session is allowed to
     * transfer to/from its IOGateway's DataIO during the next I/O stage of our event loop.
     * Called after all the calls to OkayToTransfer() have been done.
     * Called once for each session that we previously returned true for from our
     * OkayToTransfer() method.
     * You may return MUSCLE_NO_LIMIT if you want the session to be able to transfer as
     * much as it likes, or 0 if we want the session to not transfer anything at all (or
     * any number in between, of course).
     * (If you are returning 0 here, it's usually better to just have
     *  OkayToTransfer() return false for this PolicyHolder instead...
     *  that way the server won't keep waking up to service a session that won't allowed
     *  to transfer anything anyway)
     * @param holder A session to get a limit for.  Guaranteed to be one of our current
     *               PolicyHolders, and attached to the server.
     * @returns The max number of bytes the session is allowed to transfer, for now.
     *          This value will be passed in to the session's gateway's DoInput()
     *          or DoOutput() method, as appropriate.
     */
   virtual uint32 GetMaxTransferChunkSize(const PolicyHolder & holder) = 0;

   /** Called to notify you that the given session transferred the given
     * number of bytes to/from its DataIO object.
     * @param holder A session which transferred some bytes.  Guaranteed to be one of our
     *               PolicyHolders, and attached to the server.
     * @param numBytes How many bytes it transferred.  This value will be less than or equal
     *                 to the max value you return previously for this session from
     *                 GetMaxReadChunkSize().
     */
   virtual void BytesTransferred(const PolicyHolder & holder, uint32 numBytes) = 0;

   /** Called once at the end of each pass through our I/O event loop.
     * @param now The time at which the loop is ending, for convenience.
     */
   virtual void EndIO(uint64 now) = 0;

private:
   friend class ReflectServer;
   bool _hasBegun;  // used by the ReflectServer
};
DECLARE_REFTYPES(AbstractSessionIOPolicy);

}; // end namespace muscle

// At the bottom to avoid circular-forward-reference problems, while
// still allowing subclasses to automatically get this header
#include "reflector/AbstractReflectSession.h"

#endif
