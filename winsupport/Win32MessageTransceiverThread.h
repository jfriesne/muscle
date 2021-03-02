/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleWin32MessageTransceiverThread_h
#define MuscleWin32MessageTransceiverThread_h

#include "system/MessageTransceiverThread.h"

namespace muscle {

static const UINT WIN32MTT_SIGNAL_EVENT = WM_USER;  // default signal value

/**
 *  This is a Win32-API-specific subclass of MessageTransceiverThread.
 *  It can be useful when you are using MUSCLE in a Win32-only application.
 */
class Win32MessageTransceiverThread : public MessageTransceiverThread
{
public:
   /** This constructor creates an object that will signal your thread by calling
     * PostThreadMessage() with the arguments you specify here.
     * @param replyThreadID The ID of the thread you wish notification signals
     *                       to be sent to (typically GetCurrentThreadId())
     * @param signalValue Signal value to deliver to the reply thread when notifying it of an event.
     *                    Defaults to WIN32MTT_SIGNAL_EVENT.
     */
   Win32MessageTransceiverThread(DWORD replyThreadID, UINT signalValue = WIN32MTT_SIGNAL_EVENT) : _replyThreadID(replyThreadID), _signalValue(signalValue), _signalHandle(INVALID_HANDLE_VALUE), _closeHandleWhenDone(false) {/* empty */}

   /** This constructor creates an object that will signal your thread by calling
     * SetEvent() on the handle that you specify here.
     * @param signalHandle Handle that we will call SetEvent() on whenever we want to notify
     *                     the owning thread of a pending event.
     * @param closeHandleWhenDone If true, we will call CloseHandle() on (signalHandle) in our destructor.
     *                            Otherwise, our destructor will leave (signalHandle) open.
     */
   Win32MessageTransceiverThread(::HANDLE signalHandle, bool closeHandleWhenDone) : _replyThreadID(0), _signalValue(0), _signalHandle(signalHandle), _closeHandleWhenDone(closeHandleWhenDone) {/* empty */}
   
   /**
    *  Destructor.  You will generally want to call ShutdownInternalThread()
    *  before destroying this object.
    */
   virtual ~Win32MessageTransceiverThread() {if ((_signalHandle != INVALID_HANDLE_VALUE)&&(_closeHandleWhenDone)) CloseHandle(_signalHandle);}

   /** Returns the signal HANDLE that was passed in to our constructor, or INVALID_HANDLE_VALUE if there wasn't one. */
   ::HANDLE GetSignalHandle() const {return _signalHandle;}

   /** Used to set the signal HANDLE when the constructor call isn't appropriate.
     * If set to INVALID_HANDLE_VALUE, we'll use PostThreadMessage() to signal the user thread;
     * otherwise we'll call SetEvent() on this handle to do so.
     * Any previously held handle will not be closed by this call;  if you want it closed you'll
     * need to close it manually yourself, first.
     * @param signalHandle handle to call SetEvent() on from now on, or INVALID_HANDLE_VALUE if you
     *                     wish to switch to calling SetReplyThreadID() instead.
     * @param closeHandleWhenDone If true, this object's destructor will call CloseHandle() on (signalHandle).
     *                            Otherwise, the handle will not be closed by this object, ever.
     */
   void SetSignalHandle(::HANDLE signalHandle, bool closeHandleWhenDone) {_signalHandle = signalHandle; _closeHandleWhenDone = closeHandleWhenDone;}

   /** Returns true iff we plan to call CloseHandle() on our held signal handle when we are destroyed. */
   bool GetCloseHandleWhenDone() const {return _closeHandleWhenDone;}

   /** Returns the reply thread ID that was passed in to our constructor, or 0 if there wasn't one. */
   DWORD GetReplyThreadID() const {return _replyThreadID;}

   /** Used to set the reply thread ID when the constructor call isn't appropriate. 
     * @param replyThreadID The new thread ID to call PostThreadMessage() on to signal the user thread. 
     *                      This value is only used if the SignalHandle value is set to INVALID_HANDLE_VALUE.
     */
   void SetReplyThreadID(DWORD replyThreadID) {_replyThreadID = replyThreadID;}

   /** Returns the signal value that was passed in to our constructor, or 0 if there wasn't one. */
   UINT GetSignalValue() const {return _signalValue;}

   /** Used to set the signal value when value that was set in the constructor call isn't appropriate.  
     * This value is only used if the signal handle is set to a valid value (i.e. not INVALID_HANDLE_VALUE)
     * @param signalValue Signal value to deliver to the reply thread when notifying it of an event.
     */
   void SetSignalValue(UINT signalValue) {_signalValue = signalValue;}

protected:
   /** Overridden to send a signal the specified windows thread */
   virtual void SignalOwner() 
   {
      if (_signalHandle != INVALID_HANDLE_VALUE) SetEvent(_signalHandle); 
                                            else PostThreadMessage(_replyThreadID, _signalValue, 0, 0);
   }

private:
   // method 1 -- via PostThreadMessage()
   DWORD _replyThreadID;
   UINT _signalValue;

   // method 2 -- via SetEvent()
   ::HANDLE _signalHandle;
   bool _closeHandleWhenDone;

   DECLARE_COUNTED_OBJECT(Win32MessageTransceiverThread);
};

/** Here is some example code showing how to process events from a Win32MessageTransceiverThread object
  * From inside a Win32 thread, using the PostThreadMessage()/PeekMessage() method:
  *
  * Win32MessageTransceiverThread * mtt = new Win32MessageTransceiverThread(GetCurrentThreadId());
  * if ((mtt->AddNewConnectSession("beshare.tycomsystems.com", 2960).IsOK())&&(mtt->StartInternalThread().IsOK()))
  * {
  *    while(1)
  *    {
  *       MSG msg;
  *       if (PeekMessage(&msg, 0, WIN32MTT_SIGNAL_EVENT, WIN32MTT_SIGNAL_EVENT, PM_REMOVE) != 0)
  *       {
  *          if (msg.message == WIN32MTT_SIGNAL_EVENT)
  *          {
  *             uint32 code;
  *             MessageRef nextMsgRef;
  *
  *             // Check for any new messages from our internal thread
  *             while (myMessageTransceiverThread->GetNextEventFromInternalThread(code, &nextMsgRef) >= 0)
  *             {
  *                switch (code)
  *                {
  *                   case MTT_EVENT_INCOMING_MESSAGE:
  *                      printf("Received Message from network!\n");
  *                      if (nextMsgRef()) nextMsgRef()->PrintToStream();
  *                   break;
  *                
  *                   case MTT_EVENT_SESSION_CONNECTED:
  *                      printf("Connected to remote peer complete!\n");
  *                   break;
  *
  *                   case MTT_EVENT_SESSION_DISCONNECTED:
  *                      printf("Disconnected from remote peer, or connection failed!\n");
  *                   break;
  *
  *                   // other MTT_EVENT_*    handling code could be put here
  *                }
  *             }
  *          }
  *       }
  *    }
  * }
  * mtt->ShutdownInternalThread();
  * delete mtt;
  *
***/

/** And here is some example code showing how to process events from a Win32MessageTransceiverThread object
  * From inside a Win32 thread, using the SetEvent()/WaitForMultipleObjects() method:
  *
  * Win32MessageTransceiverThread * mtt = new Win32MessageTransceiverThread(CreateEvent(0, false, false, 0), true);
  * if ((mtt->AddNewConnectSession("beshare.tycomsystems.com", 2960).IsOK())&&(mtt->StartInternalThread().IsOK()))
  * {
  *    while(1)
  *    {
  *       ::HANDLE events[] = {mtt->GetSignalHandle()};  // other things to wait on can be added here too if you want
  *       switch(WaitForMultipleObjects(ARRAYITEMS(events), events, false, INFINITE)-WAIT_OBJECT_0)
  *       {
  *          case 0:
  *          {
  *             // wakeupSignal signalled!!  Check for any new messages from our internal thread
  *             uint32 code;
  *             MessageRef nextMsgRef;
  *             while (myMessageTransceiverThread->GetNextEventFromInternalThread(code, &nextMsgRef) >= 0)
  *             {
  *                switch (code)
  *                {
  *                   case MTT_EVENT_INCOMING_MESSAGE:
  *                      printf("Received Message from network!\n");
  *                      if (nextMsgRef()) nextMsgRef()->PrintToStream();
  *                   break;
  *                
  *                   case MTT_EVENT_SESSION_CONNECTED:
  *                      printf("Connected to remote peer complete!\n");
  *                   break;

  *                   case MTT_EVENT_SESSION_DISCONNECTED:
  *                      printf("Disconnected from remote peer, or connection failed!\n");
  *                   break;
  *
  *                   // other MTT_EVENT_*    handling code could be put here
  *                }
  *             }
  *             break;
  *          }
  *       }
  *    }
  * }
  * mtt->ShutdownInternalThread();
  * delete mtt;
  *
***/

} // end namespace muscle

#endif
