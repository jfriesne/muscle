#ifndef AdvancedQMessageTransceiverThread_h
#define AdvancedQMessageTransceiverThread_h

#include "qtsupport/QMessageTransceiverThread.h"

using namespace muscle;

// The port number this example program will receive TCP connections on
enum {
   ADVANCED_EXAMPLE_PORT = 2961
};

// what-codes in this range are intended to be processed by
// the sessions in the MUSCLE thread
enum {
   FIRST_ADVANCED_COMMAND = 1097102947, // 'Advc' 

   ADVANCED_COMMAND_ENDSESSION = FIRST_ADVANCED_COMMAND,
   // [... more commands for the MUSCLE thread to process could be added here...]

   AFTER_LAST_ADVANCED_COMMAND
};

// what-codes in this range are intended to be processed by
// the internal/slave threads that were spawned by the 
// ThreadedInternalSessions that are held by the MUSCLE thread.
enum {
   FIRST_INTERNAL_THREAD_COMMAND = 1231975540, // 'Intt'  

   INTERNAL_THREAD_COMMAND_HURRYUP = FIRST_INTERNAL_THREAD_COMMAND,
   // [... more commands for the internal/slave threads to process could be added here...]

   AFTER_LAST_INTERNAL_THREAD_COMMAND
};

// This is a subclass of the MUSCLE QMessageTransceiverThread object that knows
// how to do the custom qt_advanced_example logic we requiret
class AdvancedQMessageTransceiverThread : public QMessageTransceiverThread
{
public:
   AdvancedQMessageTransceiverThread();

   /** Tells the MUSCLE thread to add a new internal session to
     * the server.
     * @param args This Message can contain whatever initialization
     *             arguments need to be passed to the new session.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t AddNewThreadedInternalSession(const MessageRef & args);

protected:
   // Overridden just to re-enable logging messages for debugging purposes
   virtual ReflectServerRef CreateReflectServer();

   // Overridden to make the supervisor session a 
   // AdvancedThreadSupervisorSession so we can include our logic
   virtual ThreadSupervisorSessionRef CreateSupervisorSession();

   // Overridden to make the supervisor session a 
   // AdvancedThreadWorkerSession so we can include our logic
   virtual ThreadWorkerSessionRef CreateDefaultWorkerSession();
};

/** This subclass of ThreadWorkerSession is specialized to give us regular StorageReflectSession semantics,
  * rather than the usual ThreadWorkerSession semantics (which are more appropriate for regular client/server
  * workflows, where the server is not being run inside a GUI app).
  */
class AdvancedThreadWorkerSession : public ThreadWorkerSession
{
public:
   AdvancedThreadWorkerSession() {/* empty */}

   // Overridden to specially handle messages coming from the ThreadSupervisorSession (and therefore by extension, from the GUI thread)
   virtual void MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msgRef, void * userData);
};

/** This supervisor session is subclassed so we can add our own logic to it
  * if we want to (currently we don't need to do that) 
  */
class AdvancedThreadSupervisorSession : public ThreadSupervisorSession
{
public:
   AdvancedThreadSupervisorSession() {/* empty */}
};

#endif
