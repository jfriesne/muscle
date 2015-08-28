/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleAThread_h
#define MuscleAThread_h

#include <util/messenger.h>
#include "system/Thread.h"

namespace muscle {

/** 'what' code sent to our owner when we need to signal him */
enum {
   MUSCLE_THREAD_SIGNAL = 1299408750  /**< 'Msgn' -- sent to the main thread when messages are ready for pickup */
};

/** 
  * This class is templated to use as a AtheOS-specific subclass of any
  * MUSCLE Thread subclass.  It modifies its base class to send os::Messages
  * instead of writing to a TCP socket when it wants to notify the main thread.
  * BaseThread would typically be MessageTransceiverThread, AcceptSocketsThread, or the like.
  */
template <class BaseThread> class AThread : public BaseThread
{
public:
   /** Default Constructor.  If you use this constructor, you will want to call 
     * SetTarget() and/or SetNotificationMessage() to set where the internal thread's 
     * reply os::Messages are to go.
     */
   AThread() {SetNotificationMessageAux(NULL);}

   /** Constructor.
     * @param target os::Messenger indicating where events-ready notifications should be
     *                          sent to.  Equivalent to calling SetTarget() on this object.
     */
   AThread(const os::Messenger & target) : _target(target) {SetNotificationMessageAux(NULL);}

   /** Constructor.
     * @param target os::Messenger indicating where events-ready notifications should be
     *                          sent to.  Equivalent to calling SetTarget() on this object.
     * @param notifyMsg Message to send to notify the main thread that there is something to check.
     */
   AThread(const os::Messenger & target, const os::Message & notifyMsg) : _target(target) {SetNotificationMessageAux(&notifyMsg);}

   /** Destructor.  If the internal thread was started, you must make sure it has been
     * shut down by calling ShutdownThread() before deleting this object.
     */
   virtual ~AThread() {/* empty */}

   /** Sets our current target os::Messenger (where the internal thread will send its event 
     * notification messages.  Thread safe, so this may be called at any time.
     * @param newTarget new destination for the MUSCLE_THREAD_SIGNAL signal os::Messages.
     * @param optNewNotificationMessage If non-NULL, this pointer will be used to change the notification
     *                                  message also.  Defaults to NULL.  (see SetNotificationMessage())
     * @return B_NO_ERROR on success, or B_ERROR on failure (couldn't lock the lock???)
     */
   status_t SetTarget(const os::Messenger & newTarget, const os::Message * optNewNotificationMessage = NULL)
   {
      if (this->LockSignalling() == B_NO_ERROR)
      {
         _target = newTarget;
         if (optNewNotificationMessage) SetNotificationMessageAux(optNewNotificationMessage);
         this->UnlockSignalling();
         return B_NO_ERROR;
      }
      return B_ERROR;
   }

   /** Set a new message to send to when notifying the main thread that events are pending.
     * The default message is simply a message with the 'what' code set to MUSCLE_THREAD_SIGNAL.
     * An internal copy of (msg) is made, and the internal copy receives a "source" pointer
     * field that points to this object.  Thread safe, so may be called at any time. 
     * @param newMsg new os::Message to send instead of MUSCLE_THREAD_SIGNAL.
     * @return B_NO_ERROR on success, or B_ERROR on failure (couldn't lock the lock???)
     */
   status_t SetNotificationMessage(const os::Message & newMsg)
   {
      if (this->LockSignalling() == B_NO_ERROR)
      {
         SetNotificationMessageAux(&newMsg);
         this->UnlockSignalling();
         return B_NO_ERROR;
      }
      return B_ERROR;
   }

   /** Returns our current notification os::Message (send when incoming events are ready for pickup).  Thread safe. */
   const os::Messenger & GetTarget() const {return _target;}

protected: 
   /** Overridden to send a os::Message instead of doing silly TCP stuff */
   virtual void SignalOwner() {(void) _target.SendMessage(&_notificationMessage);}

private:
   void SetNotificationMessageAux(const os::Message * optMsg)
   {
      if (optMsg) _notificationMessage = *optMsg;
      else
      {
         _notificationMessage.MakeEmpty();
         _notificationMessage.SetCode(MUSCLE_THREAD_SIGNAL);
      }
      _notificationMessage.AddPointer("source", this);
   }

   os::Messenger _target;
   os::Message _notificationMessage;
};

}; // end namespace muscle

#include "system/MessageTransceiverThread.h"
#include "system/AcceptSocketsThread.h"

namespace muscle {

// typedefs for convenience
typedef AThread<MessageTransceiverThread> AMessageTransceiverThread;
typedef AThread<AcceptSocketsThread> AAcceptSocketsThread;

}; // end namespace muscle

#endif
