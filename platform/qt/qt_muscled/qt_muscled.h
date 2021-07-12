#ifndef MuscledWindow_h
#define MuscledWindow_h

#include <QWidget>

#include "dataio/ChildProcessDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"

class QPlainTextEdit;
class QSocketNotifier;

using namespace muscle;

class MuscledWindow : public QWidget, public AbstractGatewayMessageReceiver
{
Q_OBJECT

public:
   explicit MuscledWindow(const char * argv0);
   virtual ~MuscledWindow();

private slots:
   void TextAvailableFromChildProcess();

protected:
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData);

private:
   QPlainTextEdit * _muscledStdoutText;
   ChildProcessDataIO _cpdio;
   PlainTextMessageIOGateway _gateway;  // used to assemble full text lines
   QSocketNotifier * _notifier;
};

#endif
