#include <stdlib.h>
#include <time.h>

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QPainter>
#include <QSplitter>

#include "dataio/FileDataIO.h"
#include "message/Message.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"  // for ParseConnectArg()
#include "util/Hashtable.h"

#include "qt_example.h"

ExampleWidget :: ExampleWidget(ExampleWindow * master, bool animate)
   : _master(master)
   , _isMousePressed(false)
   , _updatePos(rand()%10000)
   , _xRatio(0.7f)
   , _yRatio(0.7f)
{
   setMinimumSize(200, 20);
   connect(&_autoUpdateTimer, SIGNAL(timeout()), this, SLOT(AutoUpdate()));
   UpdateLocalPosition();
   SetAnimateEnabled(animate);
}

void ExampleWidget :: SetAnimateEnabled(int s)
{
   if (s) _autoUpdateTimer.start(50);
     else _autoUpdateTimer.stop();
}

void ExampleWidget :: paintEvent(QPaintEvent *)
{
   QPainter p(this);
   p.setRenderHint(QPainter::Antialiasing);
   p.setRenderHint(QPainter::TextAntialiasing);

   if (_master->_isConnected)
   {
      p.fillRect(QRect(0,0,width(),height()), Qt::darkGray);

      p.setPen(Qt::gray);
      DrawText(p, NormalizedToQtCoords(Point(0.5f, 0.25f)), "Each connected qt_example client can",  Qt::gray, false);
      DrawText(p, NormalizedToQtCoords(Point(0.5f, 0.50f)), "click and drag in this area",           Qt::gray, false);
      DrawText(p, NormalizedToQtCoords(Point(0.5f, 0.75f)), "and the other clients will all see it", Qt::gray, false);

      // First, draw lines from our local position to all the other user's positions, just cause it looks cool
      QPoint myPt = NormalizedToQtCoords(_master->_localState()->GetPoint("position"));
      p.setPen(Qt::black);
      for (HashtableIterator<String, MessageRef> iter(_master->_states); iter.HasData(); iter++) p.drawLine(myPt, NormalizedToQtCoords(iter.GetValue()()->GetPoint("position")));

      // And finally draw everyone's position-indicator bubble
      for (HashtableIterator<String, MessageRef> iter(_master->_states); iter.HasData(); iter++) DrawUser(p, iter.GetValue());
      DrawUser(p, _master->_localState);
   }
   else 
   {
      p.fillRect(QRect(0,0,width(),height()), Qt::lightGray);
      DrawText(p, NormalizedToQtCoords(Point(0.5f, 0.5f)), "(Not currently connected to server)", Qt::darkGray, false);
   }
}

void ExampleWidget :: DrawUser(QPainter & p, const MessageRef & data)
{
   if (data()) DrawText(p, NormalizedToQtCoords(data()->GetPoint("position")), data()->GetString("username")(), QColor(QRgb(data()->GetInt32("color"))), true);
}

void ExampleWidget :: DrawText(QPainter & p, const QPoint & pt, const QString & text, const QColor & c, bool inBox)
{
   QFontMetrics fm = p.fontMetrics();
#if QT_VERSION >= 0x050B00
   const int tw = fm.horizontalAdvance(text);
#else
   const int tw = fm.width(text);
#endif
   const int th = fm.ascent()+fm.descent();
   QRect r(pt.x()-(tw/2), pt.y()-(th/2), tw, th);
   if (inBox)
   {
      p.setPen(Qt::black);
      p.setBrush(c);
      p.drawRoundedRect(r.adjusted(-5,-3,5,3), 10.0, 10.0);
   }

   // draw the text centered about (pt)
   p.setPen(inBox?Qt::black:c);
   p.setBrush(Qt::NoBrush);
   p.drawText(r, Qt::AlignCenter, text);
}

void ExampleWidget :: AutoUpdate()
{
   if (_isMousePressed == false)
   {
      _updatePos += 0.05f;
      UpdateLocalPosition();
   }
}

static float xform(bool isX, float updatePos, float ratio) {return 0.5f+(ratio*(((isX?cos(updatePos):sin(updatePos)))/2.0f));}
static float unxform(bool isX, float updatePos, float x)   {return ((2.0f*x)-1.0f)/((isX?cos(updatePos):sin(updatePos)));}

void ExampleWidget :: UpdateLocalPosition()
{
   SetLocalPosition(Point(xform(true, _updatePos, _xRatio), xform(false, _updatePos, _yRatio)));
}

void ExampleWidget :: mousePressEvent(QMouseEvent * e)
{
   QWidget::mousePressEvent(e);
   _isMousePressed = true;
   SetLocalPosition(QtCoordsToNormalized(e->pos()));
}

void ExampleWidget :: mouseMoveEvent(QMouseEvent * e)
{
   QWidget::mouseMoveEvent(e);
   if (_isMousePressed) SetLocalPosition(QtCoordsToNormalized(e->pos()));
}

void ExampleWidget :: mouseReleaseEvent(QMouseEvent * e)
{
   QWidget::mouseReleaseEvent(e);
   _isMousePressed = false;

   Point p((((float)e->x())/width())-0.5f, (((float)e->y())/height())-0.5f);
   _updatePos = atan2(p.y(), p.x());
   _xRatio    = unxform(true,  _updatePos, ((float)e->x())/width());
   _yRatio    = unxform(false, _updatePos, ((float)e->y())/height());
   UpdateLocalPosition();
}

void ExampleWidget :: SetLocalPosition(const Point & normPt)
{
   _localPosition = normPt;
   emit LocalPositionChanged();
   update();
}

Point ExampleWidget :: QtCoordsToNormalized(const QPoint & pt) const
{
   return Point(((float)pt.x())/width(), ((float)pt.y())/height());
}

QPoint ExampleWidget :: NormalizedToQtCoords(const Point & pt) const
{
   return QPoint((int)((pt.x()*width())+0.5f), (int)((pt.y()*height())+0.5f));
}

static QColor GetRandomBrightColor()
{
   const uint32 colorFloor = 150;
   const uint32 colorRange = (256-colorFloor);
   return QColor((rand()%colorRange)+colorFloor, (rand()%colorRange)+colorFloor, (rand()%colorRange)+colorFloor);
}

ExampleWindow :: ExampleWindow(const QString & serverName, const QString & userName, const ConstByteBufferRef & publicKey, bool animate)
   : _isConnected(false)
   , _curUserName(userName)
   , _localColor(GetRandomBrightColor())
   , _publicKey(publicKey)
{
#ifdef MUSCLE_ENABLE_SSL
   if (_publicKey()) _mtt.SetSSLPublicKeyCertificate(_publicKey);
#endif

   QPalette p = palette();
   p.setColor(QPalette::Window, _localColor);
   setPalette(p);

   setAttribute(Qt::WA_DeleteOnClose);
   resize(640, 400);

   QBoxLayout * vbl = new QBoxLayout(QBoxLayout::TopToBottom, this);
   vbl->setSpacing(3);
   vbl->setMargin(2);

   QWidget * topRow = new QWidget;
   {
      QBoxLayout * topRowLayout = new QBoxLayout(QBoxLayout::LeftToRight, topRow);
      topRowLayout->setSpacing(6);
      topRowLayout->setMargin(2);

      topRowLayout->addWidget(new QLabel("Server:"));

      _serverName = new QLineEdit;
      _serverName->setText(serverName);
      connect(_serverName, SIGNAL(returnPressed()), this, SLOT(ConnectToServer()));
      topRowLayout->addWidget(_serverName, 1);

      _connectButton = new QPushButton("Connect to Server");
      connect(_connectButton, SIGNAL(clicked()), this, SLOT(ConnectToServer()));
      topRowLayout->addWidget(_connectButton);

      _disconnectButton = new QPushButton("Disconnect from Server");
      connect(_disconnectButton, SIGNAL(clicked()), this, SLOT(DisconnectFromServer()));
      topRowLayout->addWidget(_disconnectButton);

      _cloneButton = new QPushButton("Clone Window");
      connect(_cloneButton, SIGNAL(clicked()), this, SLOT(CloneWindow()));
      topRowLayout->addWidget(_cloneButton);

      _animate = new QCheckBox("Animate");
      _animate->setChecked(animate);
      topRowLayout->addWidget(_animate);
   }
   vbl->addWidget(topRow);

   QSplitter * splitter = new QSplitter;
   splitter->setOrientation(Qt::Vertical);
   {
      _exampleWidget = new ExampleWidget(this, animate);
      connect(_exampleWidget, SIGNAL(LocalPositionChanged()), this, SLOT(UploadLocalState()));
      connect(_animate, SIGNAL(stateChanged(int)), _exampleWidget, SLOT(SetAnimateEnabled(int)));
      splitter->addWidget(_exampleWidget);

      QWidget * splitBottom = new QWidget;
      {
         QBoxLayout * splitBottomLayout = new QBoxLayout(QBoxLayout::TopToBottom, splitBottom);
         splitBottomLayout->setMargin(2);
         splitBottomLayout->setSpacing(2);
 
         _chatText = new QTextEdit;
         _chatText->setReadOnly(true);
         splitBottomLayout->addWidget(_chatText, 1);

         QWidget * botRow = new QWidget;
         {
            QBoxLayout * botRowLayout = new QBoxLayout(QBoxLayout::LeftToRight, botRow);
            botRowLayout->setSpacing(3);
            botRowLayout->setMargin(3);

            _userName = new QLineEdit;
            _userName->setText(_curUserName);
            _userName->setMinimumWidth(100);
            connect(_userName, SIGNAL(editingFinished()), this, SLOT(UserChangedName()));
            connect(_userName, SIGNAL(returnPressed()), this, SLOT(UserChangedName()));
            botRowLayout->addWidget(_userName);
            botRowLayout->addWidget(new QLabel(":"));
       
            _chatEntry = new QLineEdit;
            connect(_chatEntry, SIGNAL(returnPressed()), this, SLOT(SendChatText()));
            botRowLayout->addWidget(_chatEntry, 1);
         }
         splitBottomLayout->addWidget(botRow);
      }
      splitter->addWidget(splitBottom);
   }
   vbl->addWidget(splitter);
   
   connect(&_mtt, SIGNAL(SessionConnected(const String &, const IPAddressAndPort &)), this, SLOT(SessionConnected()));
   connect(&_mtt, SIGNAL(MessageReceived(const MessageRef &, const String &)), this, SLOT(MessageReceived(const MessageRef &)));
   connect(&_mtt, SIGNAL(SessionDisconnected(const String &)), this, SLOT(SessionDisconnected()));
   UpdateButtons();

   ConnectToServer();  // we might as well connect automatically on startup
}

ExampleWindow :: ~ExampleWindow()
{
   _mtt.ShutdownInternalThread();
}

void ExampleWindow :: UserChangedName()
{
   if (_userName->text() != _curUserName)
   {
      _curUserName = _userName->text();
      UploadLocalState();
      _exampleWidget->update();
   }
}

void ExampleWindow :: UpdateButtons()
{
   _chatEntry->setEnabled(_isConnected);
   _connectButton->setEnabled(!_isConnected);
   _disconnectButton->setEnabled(_isConnected);
   _serverName->setEnabled(!_isConnected);
   _exampleWidget->setEnabled(_isConnected);
   _exampleWidget->update();
}

static String FromQ(const QString & s)
{
   return String(s.toUtf8().constData());
}

void ExampleWindow :: CloneWindow()
{
   String newUserName = FromQ(_userName->text());

   const char * p = newUserName()+newUserName.Length();  // points to the NUL byte
   while((p>newUserName())&&(muscleInRange(*(p-1), '0', '9'))) p--;

   // Now (p) is pointing to the first digit at the end of the string
   if (muscleInRange(*p, '0', '9'))
   {
      int i = atoi(p);
      newUserName = newUserName.Substring(0, p-newUserName())+String("%1").Arg(i+1);
   }
   else newUserName += " #2";

   ExampleWindow * clone = new ExampleWindow(_serverName->text(), newUserName(), _publicKey, _exampleWidget->IsAnimateEnabled());
   clone->move(pos().x()+30, pos().y()+30);
   clone->show(); 
}

void ExampleWindow :: ConnectToServer()
{
   _isConnected = false;
   _mtt.Reset();

   String hostname;
   uint16 port = 2960;  // default port for muscled
   if (ParseConnectArg(FromQ(_serverName->text()), hostname, port).IsError()) 
   {
      AddChatText(QString("Unable to parse server name %1.").arg(_serverName->text()));
   }
   else if ((_mtt.AddNewConnectSession(hostname, port, false, SecondsToMicros(1)).IsOK())&&(_mtt.StartInternalThread().IsOK()))
   {
      AddChatText(QString("Connecting to server %1...").arg(_serverName->text()));
   }
   else AddChatText(QString("Could not initiate connection to server %1.").arg(_serverName->text()));

   UpdateButtons();
}

void ExampleWindow :: DisconnectFromServer()
{
   _isConnected = false;
   _mtt.Reset();
   AddChatText("Disconnected from server.");
   UpdateButtons();
}

void ExampleWindow :: SessionConnected()
{
   _isConnected = true;
   UpdateButtons();

   // Subscribe to the data published by other ExampleClients
   MessageRef subscribeMsg = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);
   subscribeMsg()->AddBool("SUBSCRIBE:qt_example/state", true);
   _mtt.SendMessageToSessions(subscribeMsg);

   // And upload our current state to the server
   UploadLocalState();

   AddChatText(QString("Connected to server %1").arg(_serverName->text()));
}

void ExampleWindow :: AddChatText(const QString & text)
{
   _chatText->append(text);
   _chatText->verticalScrollBar()->setValue(_chatText->verticalScrollBar()->maximum());
}

void ExampleWindow :: UploadLocalState()
{
   MessageRef stateMsg = GetMessageFromPool();
   stateMsg()->AddString("username", FromQ(_curUserName));
   stateMsg()->AddPoint("position", _exampleWidget->GetLocalPosition());
   stateMsg()->AddInt32("color", (int32) (_localColor.rgb()));

   MessageRef uploadMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
   uploadMsg()->AddMessage("qt_example/state", stateMsg);

   _localState = stateMsg;
   if (_isConnected) _mtt.SendMessageToSessions(uploadMsg);
}

enum {
   QT_EXAMPLE_CHAT_TEXT = 6666  // arbitrary
};

void ExampleWindow :: MessageReceived(const MessageRef & msg)
{
   switch(msg()->what)
   {
      case QT_EXAMPLE_CHAT_TEXT:
      {
         String fromUser = msg()->GetString("username");
         String text     = msg()->GetString("text");
         AddChatText(QString("[%1] said: %2").arg(fromUser()).arg(text()));
      }
      break;

      case PR_RESULT_DATAITEMS:
      {
         // Look for strings that indicate that subscribed nodes were removed from the tree
         {
            const String * nodePath;
            for (int i=0; (msg()->FindString(PR_NAME_REMOVED_DATAITEMS, i, &nodePath).IsOK()); i++)
            {
               MessageRef existingState;
               if (_states.Remove(*nodePath, existingState).IsOK())
               {
                  AddChatText(QString("[%1] has disconnected from the server.").arg(existingState()->GetString("username")()));
                  _exampleWidget->update();
               }
            }
         }

         // And then look for sub-messages that represend subscribed nodes that were added to the tree, or updated in-place
         {
            for (MessageFieldNameIterator iter = msg()->GetFieldNameIterator(B_MESSAGE_TYPE); iter.HasData(); iter++)
            {
               MessageRef data;
               for (uint32 i=0; msg()->FindMessage(iter.GetFieldName(), i, data).IsOK(); i++)
               {
                  if (_states.ContainsKey(iter.GetFieldName()) == false) AddChatText(QString("[%1] has connected to the server.").arg(data()->GetString("username")()));
                  _states.Put(iter.GetFieldName(), data);
               }
               _exampleWidget->update();
            }
         }
      }
      break;
   }
}

void ExampleWindow :: SendChatText()
{
   QString text = _chatEntry->text();
   _chatEntry->setText(QString());

   MessageRef chatMsg = GetMessageFromPool(QT_EXAMPLE_CHAT_TEXT);
   chatMsg()->AddString("username", FromQ(_curUserName));  // tag Message with who sent it
   chatMsg()->AddString("text", FromQ(text));
   chatMsg()->AddString(PR_NAME_KEYS, "qt_example");  // make sure the chat text only goes to qt_example clients on the server
   MessageReceived(chatMsg);  // handle it locally also (the server won't send it back to us, by default)

   _mtt.SendMessageToSessions(chatMsg);
}

void ExampleWindow :: SessionDisconnected()
{
   _isConnected = false;
   UpdateButtons();

   _states.Clear();
   _exampleWidget->update();

   AddChatText("Disconnected from server!");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;  // necessary for MUSCLE initialization
   QApplication app(argc, argv);

   srand(time(NULL));

   ByteBufferRef publicKey;
   for (int i=1; i<argc; i++)
   {
      const char * a = argv[i];
      if (strncmp(a, "publickey=", 10) == 0)
      {
         a += 10;  // skip past the 'publickey=' part

#ifdef MUSCLE_ENABLE_SSL
         FileDataIO fdio(muscleFopen(a, "rb"));
         ByteBufferRef fileData = GetByteBufferFromPool((uint32)fdio.GetLength());
         if ((fdio.GetFile())&&(fileData())&&(fdio.ReadFully(fileData()->GetBuffer(), fileData()->GetNumBytes()) == fileData()->GetNumBytes()))
         {
            LogTime(MUSCLE_LOG_INFO, "Using public key file [%s] to register with server\n", a);
            publicKey = fileData;
         }
         else
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't load public key file [%s] (file not found?)\n", a);
            return 10;
         }
#else
         LogTime(MUSCLE_LOG_CRITICALERROR, "Can't load public key file [%s], SSL support is not enabled!\n", a);
         return 10;
#endif
      }
   }

   ExampleWindow * fw = new ExampleWindow("localhost:2960", "Anonymous", publicKey, false);
   fw->show();
   return app.exec();
}
