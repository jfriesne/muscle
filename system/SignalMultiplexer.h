/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSignalMultiplexer_h
#define MuscleSignalMultiplexer_h

#include "system/Mutex.h"
#include "system/AtomicCounter.h"
#include "util/Queue.h"

namespace muscle {

/** This class is an interface that indicates an object that can receive signals from the SignalMultiplexer object. */
class ISignalHandler
{
public:
   /** Default constructor */
   ISignalHandler() {/* empty */}

   /** Destructor */
   virtual ~ISignalHandler() {/* empty */}

   /** Retrieves the nth signal number that we should try to catch and respond to.
     * @param n The index of the signal number that is to be returned (eg 0, 1, ... n)
     * @param signalNumber on success, the signal number should be written here.
     * @returns B_NO_ERROR on success, or B_BAD_ARGUMENT if there is no (nth) signal number.
     *
     * Default implementation will return SIGINT, SIGTERM, and SIGHUP under Posix OS's,
     * and CTRL_CLOSE_EVENT, CTRL_LOGOFF_EVENT, and CTRL_SHUTDOWN_EVENT under Windows.
     */
   virtual status_t GetNthSignalNumber(uint32 n, int & signalNumber) const;

   /** This callback is called by the SignalMultiplexer object.
     * Note that this callback is called directly from the signal handler,
     * and therefore most function calls are unsafe to call from here.
     * Typically you would want to have this function simply set a flag
     * or write a byte onto a socket, and do all the actual work somewhere else.
     * @param whichSignal ID of the signal we just received (meaning is OS-specific)
     */
   virtual void SignalHandlerFunc(int whichSignal) = 0;
};

/** This class is a singleton class that handles subscribing to POSIX signals (or under Windows, Console signals).
  * It is typically not used directly; instead you would typically instantiate a SignalHandlerSession and add it
  * to your ReflectServer.
  */
class SignalMultiplexer MUSCLE_FINAL_CLASS
{
public:
   /** Adds the given ISignalHandler object to our list of objects that wish to be called when a signal is raised.
     * The object's GetNthSignalNumber() method will be called to see which signal(s) it is interested in hearing about.
     * @param handler A handler object that wants to receive signal callbacks.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY.
     */
   status_t AddHandler(ISignalHandler * handler);

   /** Removes the given ISignalHandler object from our list of objects that wish to be called when a signal is raised.
     * @param handler A handler object that no longer wants to receive signal callbacks.
     */
   void RemoveHandler(ISignalHandler * handler);

   /** Calls all of our attached signal handlers' SignalHandlerFunc() method with the specified signal number.
     * This method is exposed as an implementation detail;  generally you would not need or want to call it directly.
     * @param sigNum A signal number (SIGINT/SIGHUP/etc)
     */
   void CallSignalHandlers(int sigNum);

   /** Returns a reference to the singleton SignalMultiplexer object. */
   MUSCLE_NODISCARD static SignalMultiplexer & GetSignalMultiplexer() {return _signalMultiplexer;}

   /** Returns the total number of signals (of all kinds) that have been received by this SignalMultiplexer object. */
   MUSCLE_NODISCARD uint32 GetTotalNumSignalsReceived() const {return _totalSignalCounts.GetCount();}

   /** Returns the total number of signals of the specified kind that have been received by this SignalMultiplexer object.
     * @param type The signal number (eg SIGINT).  Note that only signal numbers up to 31 are tracked.
     */
   MUSCLE_NODISCARD uint32 GetNumSignalsReceivedOfType(uint32 type) const {return (type<ARRAYITEMS(_signalCounts))?_signalCounts[type].GetCount():0;}

private:
   SignalMultiplexer()  {/* empty */}  // ctor is deliberately private
   ~SignalMultiplexer() {/* empty */}  // dtor is deliberately private

   status_t UpdateSignalSets();
   status_t RegisterSignals();

   void UnregisterSignals();

   Mutex _mutex;
   Queue<ISignalHandler *> _handlers;
   Queue<int> _currentSignalSet;

   AtomicCounter _totalSignalCounts;  // incremented inside the interrupt code
   AtomicCounter _signalCounts[32];   // incremented inside the interrupt code

   static SignalMultiplexer _signalMultiplexer;  // the singleton object
};

} // end namespace muscle

#endif

