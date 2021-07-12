#include <stdlib.h>
#include <time.h>

#include <QApplication>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>

#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"  // for HandleStandardDaemonArgs()
#include "util/StringTokenizer.h"

#include "qt_advanced_example.h"

// This class holds information about a MUSCLE session, for display in the GUI
class SessionListViewItem : public QListWidgetItem
{
public:
   SessionListViewItem(QListWidget * parent, const String & sessionID)
      : QListWidgetItem(parent)
      , _sessionID(sessionID)
   {
      Update();
      setText(_sessionID());
   }

   /** Returns the string that the MUSCLE thread uses to identify the session we correspond to */
   const String & GetSessionID() const {return _sessionID;}

   /** Called when data relevant to this node has been received from the MUSCLE thread
     * @param subPath relative path of the data item (underneath our session node)
     * @param optData If non-NULL, the new value of the sub-item; if a NULL reference it means the sub-item was deleted. 
     */
   void DataReceived(const String & subPath, const MessageRef & optData)
   {
      if (optData() != NULL) _data.Put(subPath, optData);
                        else _data.Remove(subPath);
      Update();
   } 

private:
   void Update()
   {
      String s = _sessionID + String(": ");

      // This is a super-minimalist display of what nodes our session has; a proper
      // program would do a better job of this (and would probably also be more specific
      // regarding what information it was looking for)
      for (HashtableIterator<String, MessageRef> iter(_data); iter.HasData(); iter++)
      {
         // Show the sub-path
         s += iter.GetKey();

         // And if there is a "count" integer in the Message data, show it too
         int32 count;
         if (iter.GetValue()()->FindInt32("count", count).IsOK()) s += String("=%1").Arg(count);
         s += ' ';
      }
      
      setText(s());
   }
  
   String _sessionID;
   Hashtable<String, MessageRef> _data;
};

AdvancedExampleWindow :: AdvancedExampleWindow()
{
   setWindowTitle("Qt Advanced QMessageTransceiverThread Example");
   setAttribute(Qt::WA_DeleteOnClose);
   resize(640, 400);

   QBoxLayout * vbl = new QBoxLayout(QBoxLayout::TopToBottom, this);
   vbl->setSpacing(3);
   vbl->setMargin(2);

   _sessionsView = new QListWidget;
   _sessionsView->setSelectionMode(QAbstractItemView::ExtendedSelection);
   connect(_sessionsView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(UpdateButtons()));
   vbl->addWidget(_sessionsView, 1);

   QLabel * l1 = new QLabel("Note:  Connect one or more MUSCLE clients (e.g. portablereflectclient or qt_muscled_browser)");
   l1->setAlignment(Qt::AlignCenter);
   vbl->addWidget(l1);

   QLabel * l2 = new QLabel(QString("to 127.0.0.1:%1, or create InternalThreadSessions using the buttons below").arg(ADVANCED_EXAMPLE_PORT));
   l2->setAlignment(Qt::AlignCenter);
   vbl->addWidget(l2);

   QWidget * buttonRow = new QWidget;
   {
      QBoxLayout * buttonRowLayout = new QBoxLayout(QBoxLayout::LeftToRight, buttonRow);
      buttonRowLayout->setSpacing(6);
      buttonRowLayout->setMargin(2);

      _addInternalSessionButton = new QPushButton("Add InternalThreadSession");
      connect(_addInternalSessionButton, SIGNAL(clicked()), this, SLOT(AddInternalSessionButtonClicked()));
      buttonRowLayout->addWidget(_addInternalSessionButton);

      _removeSelectedSessionsButton = new QPushButton("Remove Selected Sessions");
      connect(_removeSelectedSessionsButton, SIGNAL(clicked()), this, SLOT(RemoveSelectedSessionsButtonClicked()));
      buttonRowLayout->addWidget(_removeSelectedSessionsButton);

      _grabCurrentStateButton = new QPushButton("Grab Current State");
      connect(_grabCurrentStateButton, SIGNAL(clicked()), this, SLOT(GrabCurrentStateButtonClicked()));
      buttonRowLayout->addWidget(_grabCurrentStateButton);

      _sendMessageToSelectedSessionsButton = new QPushButton("Send Message to Selected Sessions");
      connect(_sendMessageToSelectedSessionsButton, SIGNAL(clicked()), this, SLOT(SendMessageToSelectedSessionsButtonClicked()));
      buttonRowLayout->addWidget(_sendMessageToSelectedSessionsButton);
   }
   vbl->addWidget(buttonRow);

   connect(&_serverThread, SIGNAL(MessageReceived(const MessageRef &, const String &)), this, SLOT(MessageReceivedFromServer(const MessageRef &, const String &)));
   UpdateButtons();

   // start the MUSCLE thread running
   if (_serverThread.StartInternalThread().IsOK())
   {
      // Tell the ThreadSuperVisorSession in the MUSCLE thread that we want to know about updates to session status nodes
      MessageRef subscribeMsg = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);
      subscribeMsg()->AddBool("SUBSCRIBE:/*/*", true);    // so we can see all sessions that are created or destroyed
      subscribeMsg()->AddBool("SUBSCRIBE:/*/*/*", true);  // so we can watch any nodes any session places in its "home directory" also
      if (_serverThread.SendMessageToInternalThread(subscribeMsg).IsError()) printf("Error, couldn't send subcribe message to MUSCLE thread!\n");
   }
   else printf("Error, couldn't start MUSCLE thread!\n");
}

AdvancedExampleWindow :: ~AdvancedExampleWindow()
{
   _serverThread.ShutdownInternalThread();
}

void AdvancedExampleWindow :: UpdateButtons()
{
   _removeSelectedSessionsButton->setEnabled(_sessionsView->selectionModel()->hasSelection());
   _sendMessageToSelectedSessionsButton->setEnabled(_sessionsView->selectionModel()->hasSelection());
}

// Parse out a string like "/_unknown_/7/status/float" into a session string and a sub-string,
// like "/_unknown_/7" and "status/float"
static status_t ParsePath(const String & path, String & retSessionString, String & retSubString)
{
   if (path.StartsWith('/') == false) return B_BAD_ARGUMENT;  // paranoia

   StringTokenizer tok(path(), NULL, "/");
   retSessionString  = "/";
   retSessionString += tok();  
   retSessionString += '/';
   retSessionString += tok();

   retSubString = tok.GetRemainderOfString();
   return B_NO_ERROR;
}

void AdvancedExampleWindow :: MessageReceivedFromServer(const MessageRef & msg, const String & sessionID)
{
   printf("AdvancedExampleWindow::MessageReceivedFromServer called in GUI thread! msg->what=" UINT32_FORMAT_SPEC " sessionID=[%s]\n", msg()->what, sessionID()); 

   switch(msg()->what)
   {
      case PR_RESULT_DATATREES:
      {
         // Just show the gathered data, to prove that we have it
         QPlainTextEdit * te = new QPlainTextEdit;
         te->setAttribute(Qt::WA_DeleteOnClose);
         te->setMinimumWidth(640);
         te->setWindowTitle("Grabbed MUSCLE server state");
         te->setPlainText(msg()->ToString()());
         te->show();
      }
      break;

      case PR_RESULT_DATAITEMS:
      { 
         // Look for strings that indicate that subscribed nodes were removed from the tree
         const String * removedNodePath;
         for (int i=0; (msg()->FindString(PR_NAME_REMOVED_DATAITEMS, i, &removedNodePath).IsOK()); i++)
         {
            // (removedNodePath) will be something like "/_unknown_/7/status/float"
            String sessionStr, subPath;
            if (ParsePath(*removedNodePath, sessionStr, subPath).IsOK())
            {
               if (subPath.HasChars())
               {
                  // If there is a sub-path, that means a node inside a session has been deleted, so we'll tell our corresponding SessionListViewItem about that
                  SessionListViewItem * item;
                  if (_sessionLookup.Get(sessionStr, item).IsOK())
                  {
                     printf("GUI Thread removing subPath [%s] from SessionListViewItem %p (aka [%s])\n", subPath(), item, sessionStr());
                     item->DataReceived(subPath, MessageRef());
                  }
                  else printf("GUI Thread error:  We got a notification that sub-node [%s] under session [%s] had been deleted, but we have no record for that session!\n", subPath(), sessionStr());
               }
               else
               {
                  // No sub-path means the session itself has gone away
                  SessionListViewItem * item;
                  if (_sessionLookup.Remove(sessionStr, item).IsOK())
                  {
                     printf("GUI Thread removing SessionListViewItem %p (for session [%s])\n", item, removedNodePath->Cstr());
                     delete item;
                  }
                  else printf("GUI Thread error:  We got a notification that session [%s] had been deleted, but we have no record for that session!\n", sessionStr());
               }
            }
            else printf("GUI Thread error:  Unexpected PR_NAME_REMOVED_DATAITEMS path [%s]\n", removedNodePath->Cstr());
         }

         // And then look for sub-messages that represent subscribed nodes that were added to the tree or updated in-place
         for (MessageFieldNameIterator iter = msg()->GetFieldNameIterator(B_MESSAGE_TYPE); iter.HasData(); iter++)
         {
            const String & pathStr = iter.GetFieldName(); // e.g. "/_unknown_/7/status/float"
            String sessionStr, subPath;
            if (ParsePath(pathStr, sessionStr, subPath).IsOK())
            {
               MessageRef data;
               for (int32 i=0; msg()->FindMessage(iter.GetFieldName(), i, data).IsOK(); i++)
               {
                  // Create a SessionListViewItem for this sessionStr, if we don't already have one
                  SessionListViewItem * item;
                  if (_sessionLookup.Get(sessionStr, item).IsError()) 
                  {
                     item = new SessionListViewItem(_sessionsView, sessionStr);
                     _sessionLookup.Put(sessionStr, item);
                     printf("GUI Thread adding SessionListViewItem %p (for session [%s])\n", item, sessionStr());
                  }
                  if (subPath.HasChars()) item->DataReceived(subPath, data);
               }
            }
            else printf("GUI Thread error:  Unexpected sub-Message path [%s]\n", pathStr());
         }
      }
      break;
   }
}

void AdvancedExampleWindow :: AddInternalSessionButtonClicked()
{
   // tell the AdvancedQMessageTransceiverThread to create a new AdvancedWorkerSession for us.
   printf("AddInternalSessionButtonClicked:  GUI Thread is asking the MUSCLE thread to create a new internal session...\n");

   MessageRef args = GetMessageFromPool();
   args()->AddString("Your startup instructions/parameters to pass to the internal thread", "could go here");
   args()->AddInt32("Really", 1234);

   _serverThread.AddNewThreadedInternalSession(args);
}

void AdvancedExampleWindow :: RemoveSelectedSessionsButtonClicked()
{
   QList<QListWidgetItem *> selectedItems = _sessionsView->selectedItems();
   for (int i=0; i<selectedItems.count(); i++)
   {
      SessionListViewItem * slvi = static_cast<SessionListViewItem *>(selectedItems[i]);
      
      MessageRef endSession = GetMessageFromPool(ADVANCED_COMMAND_ENDSESSION);
      endSession()->AddString(PR_NAME_KEYS, slvi->GetSessionID());  // make sure it only goes to the session we want to go away
      _serverThread.SendMessageToInternalThread(endSession);

      printf("RemoveSelectedSessionsButtonClicked: GUI Thread is asking the MUSCLE thread to remove session [%s]\n", slvi->GetSessionID()());
   }
}

// Sends a Message to all the selected sessions, telling them to hurry up!
void AdvancedExampleWindow :: SendMessageToSelectedSessionsButtonClicked()
{
   QList<QListWidgetItem *> selectedItems = _sessionsView->selectedItems();
   for (int i=0; i<selectedItems.count(); i++)
   {
      SessionListViewItem * slvi = static_cast<SessionListViewItem *>(selectedItems[i]);
      
      MessageRef pokeSession = GetMessageFromPool(INTERNAL_THREAD_COMMAND_HURRYUP);
      pokeSession()->AddString(PR_NAME_KEYS, slvi->GetSessionID());  // make sure it only goes to the session we want it to go to
      pokeSession()->AddString("hurry up", "already!");
      pokeSession()->AddInt32("count", 9);  // this will cause the internal thread to increment their counters by 9, plus one for the timeout
      _serverThread.SendMessageToInternalThread(pokeSession);

      printf("SendMessageToSelectedSessionsButtonClicked: GUI Thread is asking the MUSCLE thread to hurry up session [%s]\n", slvi->GetSessionID()());
   }
}


void AdvancedExampleWindow :: GrabCurrentStateButtonClicked()
{
   printf("GUI Thread sending a request for a snapshot of the current state from the MUSCLE thread...\n");

   MessageRef msg = GetMessageFromPool(PR_COMMAND_GETDATATREES);
   msg()->AddString(PR_NAME_KEYS, "/*");  // ask for the entire tree!
   _serverThread.SendMessageToInternalThread(msg);
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;  // necessary for MUSCLE initialization

   {
      Message args; (void) ParseArgs(argc, argv, args);
      HandleStandardDaemonArgs(args);
   }

   QApplication app(argc, argv);

   AdvancedExampleWindow * fw = new AdvancedExampleWindow;
   fw->show();
   return app.exec();
}
