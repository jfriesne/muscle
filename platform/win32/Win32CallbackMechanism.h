/* This file is Copyright 2002 Level Control Systems.  See the included LICENSE.txt file for details. */

#ifndef Win32CallbackMechanism_h
#define Win32CallbackMechanism_h

#include "util/ICallbackMechanism.h"

namespace muscle {

static const UINT WIN32CALLBACKMECHANISM_SIGNAL_CODE = WM_USER;  /// default signal value when using Win32CallbackMechanism's signal-to-a-thread constructor

/** This is a Win32-specific subclass of ICallbackMechanism.  It can call either SetEvent() or PostThreadMEssage() to notify the main thread to dispatch events. */
class Win32CallbackMechanism : public ICallbackMechanism
{
public:
   /** This constructor creates an object that will signal your thread by calling
     * PostThreadMessage() with the arguments you specify here.
     * @param replyThreadID The ID of the thread you wish notification signals
     *                       to be sent to (typically GetCurrentThreadId())
     * @param signalValue Signal value to deliver to the reply thread when notifying it of an event.
     *                    Defaults to WIN32CALLBACKMECHANISM_SIGNAL_CODE.
     */
   Win32CallbackMechanism(DWORD replyThreadID, UINT signalValue = WIN32CALLBACKMECHANISM_SIGNAL_CODE) : _replyThreadID(replyThreadID), _signalValue(signalValue), _signalHandle(INVALID_HANDLE_VALUE), _closeHandleWhenDone(false) {/* empty */}

   /** This constructor creates an object that will signal your thread by calling
     * SetEvent() on the handle that you specify here.
     * @param signalHandle Handle that we will call SetEvent() on whenever we want to notify
     *                     the owning thread of a pending event.
     * @param closeHandleWhenDone If true, we will call CloseHandle() on (signalHandle) in our destructor.
     *                            Otherwise, our destructor will leave (signalHandle) open.
     */
   Win32CallbackMechanism(::HANDLE signalHandle, bool closeHandleWhenDone) : _replyThreadID(0), _signalValue(0), _signalHandle(signalHandle), _closeHandleWhenDone(closeHandleWhenDone) {/* empty */}
   
   /**
    *  Destructor.  You will generally want to call ShutdownInternalThread()
    *  before destroying this object.
    */
   virtual ~Win32CallbackMechanism() {if ((_signalHandle != INVALID_HANDLE_VALUE)&&(_closeHandleWhenDone)) CloseHandle(_signalHandle);}

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
   /** May be called from any thread; triggers an asynchronous call to DispatchCallbacks() within the main thread */
   virtual void SignalDispatchThread()
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
};

} // end namespace muscle

#endif
