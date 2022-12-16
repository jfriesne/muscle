#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/LogCallback.h"
#include "syslog/SysLog.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates LogCallbacks for custom logging functionality.\n");
   printf("You can use callbacks to e.g. forward LogTime() output to syslogd, or to a TCP socket, or etc.\n");
   printf("\n");
}

// This callback is called whenever LogPlain() or LogTime() is called; it's a bit low-level but the most flexible */
class MyLogCallback : public LogCallback
{
public:
   MyLogCallback() {/* empty */}

   virtual void Log(const LogCallbackArgs & a)
   {
      char temp[1024];
      vsnprintf(temp, sizeof(temp), a.GetText(), *a.GetArgList());

      fprintf(stderr, "MyLogCallback::Log():  Got a severity-%i callback for text [%s]\n", a.GetLogLevel(), temp);
   }

   virtual void Flush()
   {
      // not really necessary for stderr, but e.g. if you were logging to a TCPSocketDataIO you might call
      // Flush() on it here in order to get the logging data out to the network without a Nagle-200mS-delay.
      fflush(stderr);

      fprintf(stderr, "MyLogCallback::Flush() called.\n");
   }
};

// This callback is called whenever a full line of text is available for logging.  It's less flexible but easier to use. */
class MyLogLineCallback : public LogLineCallback
{
public:
   MyLogLineCallback() {/* empty */}

   virtual void LogLine(const LogCallbackArgs & a)
   {
      char temp[1024];
      vsnprintf(temp, sizeof(temp), a.GetText(), *a.GetArgList());

      fprintf(stderr, "MyLogLineCallback::LogLine():  Got a severity-%i callback for text [%s]\n", a.GetLogLevel(), temp);
   }
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Install a per-Log()-call LogCallback, just to demonstrate with
   MyLogCallback myLogCallback;
   PutLogCallback(DummyLogCallbackRef(myLogCallback));

   // Also Install a per-line-call LogLineCallback, just to demonstrate with
   MyLogLineCallback myLogLineCallback;
   PutLogCallback(DummyLogCallbackRef(myLogLineCallback));

   // Now let's try some logging and see how our callbacks get called
   LogTime(MUSCLE_LOG_INFO, "This message was sent via a single call to LogTime()\n");

   printf("\n");

   LogTime(MUSCLE_LOG_INFO, "This message was ");
   LogPlain(MUSCLE_LOG_INFO, "sent across several ");
   LogPlain(MUSCLE_LOG_INFO, "calls to LogPlain()\n");

   // Since we declared our log callbacks on the stack, let's take care to
   // remove the callbacks before the objects are destroyed; otherwise a late call
   // to LogTime() might try to call a deleted callback object, with unfortunate results.
   RemoveLogCallback(DummyLogCallbackRef(myLogLineCallback));
   RemoveLogCallback(DummyLogCallbackRef(myLogCallback));

   return 0;
}
