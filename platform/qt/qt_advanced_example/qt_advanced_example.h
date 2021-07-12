#ifndef qt_advanced_example_h
#define qt_advanced_example_h

#include <QWidget>

#include "util/Hashtable.h"
#include "util/String.h"

#include "AdvancedQMessageTransceiverThread.h"

class QListWidget;
class QPushButton;

using namespace muscle;

class SessionListViewItem;

class AdvancedExampleWindow : public QWidget
{
Q_OBJECT

public:
   AdvancedExampleWindow();
   virtual ~AdvancedExampleWindow();

private slots:
   // Called whenever the GUI thread receives any Message from the MUSCLE server thread
   void MessageReceivedFromServer(const MessageRef & msg, const String & sessionID);

private slots:
   // Slots for GUI actions
   void AddInternalSessionButtonClicked();
   void RemoveSelectedSessionsButtonClicked();
   void GrabCurrentStateButtonClicked();
   void SendMessageToSelectedSessionsButtonClicked();
   void UpdateButtons();

private:
   AdvancedQMessageTransceiverThread _serverThread;
   QListWidget * _sessionsView;
   QPushButton * _addInternalSessionButton;
   QPushButton * _removeSelectedSessionsButton;
   QPushButton * _grabCurrentStateButton;
   QPushButton * _sendMessageToSelectedSessionsButton;

   Hashtable<String, SessionListViewItem *> _sessionLookup;
};

#if defined(MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION)
class ThreadedInternalSession;

// Just a little QObject that calls a known function when its CallSendExampleMessageToMainThread() slot
// is called.  This way I don't have to make ThreadedInternalSession derive from QObject
class TimerSignalReceiverObject : public QObject
{
Q_OBJECT
   
public:
   explicit TimerSignalReceiverObject(ThreadedInternalSession * master) : _master(master) {/* empty */}

public slots:
   void CallSendExampleMessageToMainThread();

private:
   ThreadedInternalSession * _master;
};
#endif

#endif
