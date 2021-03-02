#include <QApplication>
#include <QPlainTextEdit>
#include <QLayout>
#include <QSocketNotifier>

#include "qt_muscled.h"
#include "system/SetupSystem.h"
#include "util/Queue.h"
#include "util/String.h"

using namespace muscle;

extern int muscledmain(int, char **);  // from muscled.cpp

MuscledWindow :: MuscledWindow(const char * argv0)
   : _cpdio(false)
   , _notifier(NULL)
{
   resize(800, 400);
   setWindowTitle("MUSCLEd Server Process");

   QBoxLayout * bl = new QBoxLayout(QBoxLayout::TopToBottom, this);
   bl->setMargin(0);
   bl->setSpacing(0);

   _muscledStdoutText = new QPlainTextEdit;
   _muscledStdoutText->setReadOnly(true);
   bl->addWidget(_muscledStdoutText); 

   Queue<String> argv;
   argv.AddTail(argv0);
   argv.AddTail("muscled");   
   argv.AddTail("displaylevel=trace");
   argv.AddTail("catchsignals");
   if (_cpdio.LaunchChildProcess(argv).IsOK()) 
   {
      _gateway.SetDataIO(DataIORef(&_cpdio, false));
      _notifier = new QSocketNotifier(_cpdio.GetReadSelectSocket().GetFileDescriptor(), QSocketNotifier::Read, this);
      connect(_notifier, SIGNAL(activated(int)), this, SLOT(TextAvailableFromChildProcess()));
   }
   else _muscledStdoutText->appendPlainText("<Error launching muscled sub-process!>\r\n");
}

MuscledWindow :: ~MuscledWindow()
{
   if (_notifier) _notifier->setEnabled(false);  // avoid CPU-spins under Lion
   _gateway.SetDataIO(DataIORef());  // paranoia
}

void MuscledWindow :: TextAvailableFromChildProcess()
{
   if (_gateway.DoInput(*this) < 0)
   {
      _muscledStdoutText->appendPlainText("\r\n<muscled sub-process exited>");
      delete _notifier;
      _notifier = NULL;
   }
}

void MuscledWindow :: MessageReceivedFromGateway(const MessageRef & msg, void *)
{
   const String * s;
   for (int32 i=0; msg()->FindString(PR_NAME_TEXT_LINE, i, &s).IsOK(); i++) _muscledStdoutText->appendPlainText(s->Cstr());
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   if ((argc > 1)&&(strcmp(argv[1], "muscled") == 0)) 
   {
      // The muscled process (will be launched by the MuscledWindow constructor)
      return muscledmain(argc, argv);
   }
   else
   {
      QApplication app(argc, argv);

      // The GUI process
      MuscledWindow mw(argv[0]);
      mw.show();
      return app.exec();
   }
}
