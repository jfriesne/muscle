#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLayout>
#include <QLabel>
#include "Browser.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"  // for ParseConnectArg()

// Convenience macro
#define FromQ(qs) ((qs).toUtf8().constData())

using namespace muscle;

// Items of this class each represent one node in the server-nodes treeview.
class NodeTreeWidgetItem : public QTreeWidgetItem
{
public:
   explicit NodeTreeWidgetItem(QTreeWidget * parent)
      : QTreeWidgetItem(parent, QStringList("/")) 
{setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);}
   NodeTreeWidgetItem(NodeTreeWidgetItem * parent, const String & name) : QTreeWidgetItem(parent, QStringList(name())), _name(name) {setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);}

   NodeTreeWidgetItem * GetChildByName(const String & name)
   {
      for (int i=childCount()-1; i>=0; i--)
      {
         NodeTreeWidgetItem * ch = static_cast<NodeTreeWidgetItem *>(child(i));
         if (ch->GetName() == name) return ch;
      }
      return NULL;
   }

   const String & GetName() const {return _name;}

   String GetPath() const
   {
      const NodeTreeWidgetItem * p = static_cast<const NodeTreeWidgetItem *>(parent());
      return p ? (p->GetPath()+"/"+_name) : GetEmptyString();
   }

   void DeleteChildren()
   {
      for (int i=childCount()-1; i>=0; i--) delete child(i);
   }

private:
   String _name;
};

BrowserWindow :: BrowserWindow()
   : _isConnecting(false)
   , _isConnected(false)
{
   setAttribute(Qt::WA_DeleteOnClose);
   setWindowTitle("MUSCLE Database Browser");

   const int defaultWindowWidth  = 640;
   const int defaultWindowHeight = 400;
   resize(defaultWindowWidth, defaultWindowHeight);

   QBoxLayout * vLayout = new QBoxLayout(QBoxLayout::TopToBottom, this);
   vLayout->setMargin(2);
   vLayout->setSpacing(2);
   
   QWidget * topRow = new QWidget;
   {
      QBoxLayout * topRowLayout = new QBoxLayout(QBoxLayout::LeftToRight, topRow);
      topRowLayout->setMargin(2);

      QPushButton * cloneButton = new QPushButton("Clone Window");
      connect(cloneButton, SIGNAL(clicked()), this, SLOT(CloneWindow()));
      topRowLayout->addWidget(cloneButton);

      _stateLabel = new QLabel;
      topRowLayout->addWidget(_stateLabel);

      _serverName = new QLineEdit;
      _serverName->setText("localhost:2960");
      topRowLayout->addWidget(_serverName, 1);

      _connectButton = new QPushButton("Connect to Server");
      connect(_connectButton, SIGNAL(clicked()), this, SLOT(ConnectButtonClicked()));
      topRowLayout->addWidget(_connectButton);
   }
   vLayout->addWidget(topRow);

   QSplitter * splitter = new QSplitter(Qt::Horizontal);
   {
      _nodeTree = new QTreeWidget;
      _nodeTree->setHeaderLabels(QStringList("Server Node Tree"));
      _nodeTree->setIndentation(15);
      connect(_nodeTree, SIGNAL(itemExpanded(QTreeWidgetItem *)), this, SLOT(NodeExpanded(QTreeWidgetItem *)));
      connect(_nodeTree, SIGNAL(itemCollapsed(QTreeWidgetItem *)), this, SLOT(NodeCollapsed(QTreeWidgetItem *)));
      connect(_nodeTree, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), this, SLOT(SetMessageContentsViewContents(QTreeWidgetItem *)));
      splitter->addWidget(_nodeTree);

      _messageContents = new QTextEdit;
      _messageContents->setReadOnly(true);
      splitter->addWidget(_messageContents);

      QList<int> sizeList;
      const int defaultTreeWidth = 200;
      sizeList.push_back(defaultTreeWidth);
      sizeList.push_back(defaultWindowWidth-defaultTreeWidth);
      splitter->setSizes(sizeList);
   }
   vLayout->addWidget(splitter, 1);

   connect(&_mtt, SIGNAL(SessionConnected(const String &, const IPAddressAndPort &)), this, SLOT(ConnectedToServer()));
   connect(&_mtt, SIGNAL(MessageReceived(const MessageRef &, const String &)),        this, SLOT(MessageReceivedFromServer(const MessageRef &)));
   connect(&_mtt, SIGNAL(SessionDisconnected(const String &)),                        this, SLOT(DisconnectedFromServer()));

   ConnectButtonClicked();  // might as well try to autoconnect on startup....
}

void BrowserWindow :: ConnectedToServer()
{
   _isConnected  = true;
   _isConnecting = false;
   UpdateState();

   ClearState();

   // Tell the server we want to see our own nodes as well as everyone else's
   MessageRef msg = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);
   msg()->AddBool(PR_NAME_REFLECT_TO_SELF, true);
   _mtt.SendMessageToSessions(msg);

   // Also upload a node to the server, just for fun
   MessageRef uploadMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
   {
      MessageRef dataMsg = GetMessageFromPool();
      dataMsg()->AddString("timestamp", GetHumanReadableTimeString(GetRunTime64()));
      uploadMsg()->AddMessage("connected_at", dataMsg);
   }
   _mtt.SendMessageToSessions(uploadMsg);

   // Start by subscribing to the children of the root node only
   _nodeRoot = new NodeTreeWidgetItem(_nodeTree);  // add the root node to the node tree
   _nodeRoot->setExpanded(true);  // calls SetNodeSubscribed("", true);
}

void BrowserWindow :: SetMessageContentsViewContents(QTreeWidgetItem * item)
{
   QString t;
   if (item)
   {
      String itemPath = (static_cast<NodeTreeWidgetItem *>(item))->GetPath();
      MessageRef * m = _pathToMessage.Get(itemPath);
      if (m)
      {
         t = QString("Message at path [%1] is:\n\n").arg(itemPath());
         t += m->GetItemPointer()->ToString()();
      }
      else t = QString("Message at path [%1] isn't known").arg(itemPath());

      _messageContentsPath = itemPath;
   }
   else _messageContentsPath.Clear();

   _messageContents->setText(t);
}

void BrowserWindow :: SetNodeSubscribed(const String & nodePath, bool isSubscribe)
{
   String subscribePath = nodePath+"/*";
   if (_subscriptions.ContainsKey(subscribePath) != isSubscribe)
   {
      if (isSubscribe)
      {
         MessageRef subMsg = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);
         subMsg()->AddBool(subscribePath.Prepend("SUBSCRIBE:"), true);
         _subscriptions.PutWithDefault(subscribePath);
         LogTime(MUSCLE_LOG_INFO, "BrowserWindow %p subscribed to path [%s]\n", this, subscribePath());
         _mtt.SendMessageToSessions(subMsg);
      }
      else
      {
         MessageRef unsubMsg = GetMessageFromPool(PR_COMMAND_REMOVEPARAMETERS);
         unsubMsg()->AddString(PR_NAME_KEYS, EscapeRegexTokens(subscribePath).Prepend("SUBSCRIBE:"));
         LogTime(MUSCLE_LOG_INFO, "BrowserWindow %p unsubscribed from path [%s]\n", this, subscribePath());
         _subscriptions.Remove(subscribePath);

         // Also remove from our tree of locally-cached data any nodes that start with this path
         String removePath = nodePath + "/";
         for (HashtableIterator<String, MessageRef> iter(_pathToMessage); iter.HasData(); iter++) if (iter.GetKey().StartsWith(removePath)) 
         {
            _pathToMessage.Remove(iter.GetKey());
            LogTime(MUSCLE_LOG_INFO, "BrowserWindow %p dropped node for [%s]\n", this, iter.GetKey()());
         }

         _mtt.SendMessageToSessions(unsubMsg);
      }
   }
}

void BrowserWindow :: NodeExpanded(QTreeWidgetItem * node)
{
   String nodePath = static_cast<const NodeTreeWidgetItem *>(node)->GetPath();
   SetNodeSubscribed(nodePath, true);
}

void BrowserWindow :: NodeCollapsed(QTreeWidgetItem * node)
{
   NodeTreeWidgetItem * ntwi = static_cast<NodeTreeWidgetItem *>(node);
   String subPath = ntwi->GetPath() + "/";
   for (HashtableIterator<String, Void> iter(_subscriptions); iter.HasData(); iter++) if (iter.GetKey().StartsWith(subPath)) SetNodeSubscribed(iter.GetKey().Substring(0, iter.GetKey().Length()-2), false);
   ntwi->DeleteChildren();
}

void BrowserWindow :: ClearState()
{
   _nodeRoot = NULL;  // paranoia
   _subscriptions.Clear();
   _pathToMessage.Clear();
   _nodeTree->clear();
   _messageContents->clear();
}

NodeTreeWidgetItem * BrowserWindow :: GetNodeFromPath(const String & nodePath) 
{
   if (nodePath.StartsWith("/") == false) return NULL;  // all paths must start with "/"
   return (nodePath.Length()>1) ? GetNodeFromPathAux(_nodeRoot, nodePath()+1) : _nodeRoot;
}

NodeTreeWidgetItem * BrowserWindow :: GetNodeFromPathAux(NodeTreeWidgetItem * node, const char * path)
{
   const char * slash = strchr(path, '/');
   NodeTreeWidgetItem * child = node->GetChildByName(String(path, slash?(slash-path):MUSCLE_NO_LIMIT));
   return child ? (slash ? GetNodeFromPathAux(child, slash+1) : child) : NULL;
}

// Add or remove the modified node from its parent in the tree view, as necessary
void BrowserWindow :: UpdateDataNodeInTreeView(const String & nodePath)
{
   int lastSlash = nodePath.LastIndexOf('/');
   if (lastSlash >= 0)
   {
      String parentPath = nodePath.Substring(0, lastSlash);
      if (parentPath.Length() == 0) parentPath = "/";

      NodeTreeWidgetItem * parentItem = GetNodeFromPath(parentPath);
      if ((parentItem)&&(parentItem->isExpanded()))
      {
         NodeTreeWidgetItem * curItem = GetNodeFromPath(nodePath);
         if (_pathToMessage.ContainsKey(nodePath))
         {
            if (curItem == NULL) (void) new NodeTreeWidgetItem(parentItem, nodePath.Substring("/"));
         }
         else delete curItem;
      }
   }
   if (nodePath == _messageContentsPath)  SetMessageContentsViewContents(GetNodeFromPath(nodePath));
}

void BrowserWindow :: MessageReceivedFromServer(const MessageRef & msg)
{
   switch(msg()->what)
   {
      case PR_RESULT_DATAITEMS:
      {
         // Look for strings that indicate that subscribed nodes were removed from the tree
         {
            const String * nodePath;
            for (int i=0; (msg()->FindString(PR_NAME_REMOVED_DATAITEMS, i, &nodePath).IsOK()); i++)
            {
               if (_pathToMessage.Remove(*nodePath).IsOK())
               {
                   UpdateDataNodeInTreeView(*nodePath);
                   LogTime(MUSCLE_LOG_INFO, "BrowserWindow %p removed node at [%s]\n", this, nodePath->Cstr());
               }
            }
         }

         // And then look for sub-messages that represend subscribed nodes that were added to the tree, or updated in-place
         {
            for (MessageFieldNameIterator iter = msg()->GetFieldNameIterator(B_MESSAGE_TYPE); iter.HasData(); iter++)
            {
               const String & nodePath = iter.GetFieldName();
               MessageRef data;
               for (uint32 i=0; msg()->FindMessage(nodePath, i, data).IsOK(); i++)
               {
                  if (_pathToMessage.Put(nodePath, data).IsOK())
                  {
                     UpdateDataNodeInTreeView(nodePath);
                     LogTime(MUSCLE_LOG_INFO, "BrowserWindow %p added/updated node at [%s]\n", this, nodePath());
                  } 
               }
            }
         }
      }
      break;
   }
}

void BrowserWindow :: DisconnectedFromServer()
{
   _isConnected = _isConnecting = false;
   ClearState();
   UpdateState();
}

void BrowserWindow :: CloneWindow()
{
   BrowserWindow * clone = new BrowserWindow();
   clone->_serverName->setText(_serverName->text());
   clone->show();
}

void BrowserWindow :: ConnectButtonClicked()
{
   bool wasConnecting = ((_isConnected)||(_isConnecting));
   _isConnected = _isConnecting = false;
   _mtt.Reset();

   if (wasConnecting == false)
   {
      String serverName;
      uint16 port = 2960;
      if ((ParseConnectArg(FromQ(_serverName->text()), serverName, port).IsOK())&&(_mtt.AddNewConnectSession(serverName, port).IsOK())&&(_mtt.StartInternalThread().IsOK())) _isConnecting = true;
   }      
   UpdateState();
}

void BrowserWindow :: UpdateState()
{
   if (_isConnected)
   {
      _stateLabel->setText("Connected to:");
      _connectButton->setText("Disconnect from Server");
      _serverName->setReadOnly(true);
   }
   else if (_isConnecting)
   {
      _stateLabel->setText("Connecting to:");
      _connectButton->setText("Cancel Connection");
      _serverName->setReadOnly(true);
   }
   else
   {
      _stateLabel->setText("Not Connected to:");
      _connectButton->setText("Connect to Server");
      _serverName->setReadOnly(false);
   }

   _nodeTree->setVisible(_isConnected);
   _messageContents->setVisible(_isConnected);
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;
   QApplication app(argc, argv);

   BrowserWindow * bw = new BrowserWindow;  // must be on the heap since we call setAttribute(Qt::WA_DeleteOnClose) in the BrowserWindow constructor
   bw->show();
   return app.exec();
}
