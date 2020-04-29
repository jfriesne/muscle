#ifndef Browser_h
#define Browser_h

#include <QWidget>

class QLabel;
class QPushButton;
class QTreeWidget;
class QLineEdit;
class NodeTreeWidgetItem;
class QTextEdit;
class QTreeWidgetItem;

#include "qtsupport/QMessageTransceiverThread.h"

using namespace muscle;

class BrowserWindow : public QWidget
{
Q_OBJECT

public:
   BrowserWindow();

private slots:
   void ConnectButtonClicked();
   void ConnectedToServer();
   void MessageReceivedFromServer(const MessageRef &);
   void DisconnectedFromServer();
   void NodeExpanded(QTreeWidgetItem *);
   void NodeCollapsed(QTreeWidgetItem *);
   void SetMessageContentsViewContents(QTreeWidgetItem *);
   void CloneWindow();

private:
   void UpdateState();
   void ClearState();
   void SetNodeSubscribed(const String & nodePath, bool subscribe);
   void UpdateDataNodeInTreeView(const String & nodePath);

   NodeTreeWidgetItem * GetNodeFromPathAux(NodeTreeWidgetItem * node, const char * nodePath);
   NodeTreeWidgetItem * GetNodeFromPath(const String & nodePath);

   QLabel * _stateLabel;
   QLineEdit * _serverName;
   QPushButton * _connectButton;
   QTreeWidget * _nodeTree;
   QTextEdit * _messageContents;
   NodeTreeWidgetItem * _nodeRoot;
   String _messageContentsPath;

   Hashtable<String, Void> _subscriptions;   // our current set of subscription strings
   Hashtable<String, MessageRef> _pathToMessage;  // our current set of known Node states
   QMessageTransceiverThread _mtt;

   bool _isConnecting;
   bool _isConnected;
};

#endif
