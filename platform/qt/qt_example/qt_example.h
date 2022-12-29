#ifndef QT_EXAMPLE_H
#define QT_EXAMPLE_H

#include <QWidget>
#include <QTimer>
#include "platform/qt/QMessageTransceiverThread.h"
#include "support/Point.h"

using namespace muscle;

class QCheckBox;
class QLineEdit;
class QTextEdit;
class QPainter;
class QPushButton;
class ExampleWindow;

class ExampleWidget : public QWidget
{
Q_OBJECT

public:
   ExampleWidget(ExampleWindow * master, bool animate);

   virtual void paintEvent(QPaintEvent * e);
   virtual void mousePressEvent(QMouseEvent * e);
   virtual void mouseMoveEvent(QMouseEvent * e);
   virtual void mouseReleaseEvent(QMouseEvent * e);

   const Point & GetLocalPosition() const {return _localPosition;}
   bool IsAnimateEnabled() const {return _autoUpdateTimer.isActive();}

signals:
   void LocalPositionChanged();

public slots:
   void SetAnimateEnabled(int);

private slots:
   void AutoUpdate();

private:
   Point QtCoordsToNormalized(const QPoint & pt) const;
   QPoint NormalizedToQtCoords(const Point & pt) const;
   void DrawUser(QPainter & p, const ConstMessageRef & data);
   void DrawText(QPainter & p, const QPoint & pt, const QString & text, const QColor & color, bool inBox);
   void SetLocalPosition(const Point & p);
   void UpdateLocalPosition();

   ExampleWindow * _master;
   Point _localPosition;
   bool _isMousePressed;

   QTimer _autoUpdateTimer;
   float _updatePos;

   float _xRatio;
   float _yRatio;
};

class ExampleWindow : public QWidget
{
Q_OBJECT

public:
   ExampleWindow(const QString & serverName, const QString & userName, const ConstByteBufferRef & publicKey, bool animate);
   virtual ~ExampleWindow();

private slots:
   // slots for responding to GUI actions
   void ConnectToServer();
   void DisconnectFromServer();
   void UpdateButtons();
   void UploadLocalState();
   void UserChangedName();
   void AddChatText(const QString & text);
   void SendChatText();

   // slots called by signals from the QMessageTransceiverThread object
   void SessionConnected();
   void MessageReceived(const MessageRef & msg);
   void SessionDisconnected();

   void CloneWindow();

private:
   friend class ExampleWidget;

   bool _isConnected;

   QPushButton * _connectButton;
   QPushButton * _disconnectButton;
   QPushButton * _cloneButton;
   QLineEdit * _serverName;

   ExampleWidget * _exampleWidget;

   QTextEdit * _chatText;
   QLineEdit * _userName;
   QLineEdit * _chatEntry;

   QString _curUserName;
   QColor _localColor;
   QCheckBox * _animate;

   QMessageTransceiverThread _mtt;

   MessageRef _localState;  // contains our local state, as recently uploaded
   Hashtable<String, ConstMessageRef> _states;  // contains info we gathered about other clients, from our subscription

   ConstByteBufferRef _publicKey;
};

#endif
