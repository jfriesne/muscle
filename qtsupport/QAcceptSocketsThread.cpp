/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <QCoreApplication>
#include "qtsupport/QAcceptSocketsThread.h"

namespace muscle {

static const uint32 QMTT_SIGNAL_EVENT = QEvent::User+14836;  // why yes, this is a completely arbitrary number

#if QT_VERSION >= 0x040000
QAcceptSocketsThread :: QAcceptSocketsThread(QObject * parent, const char * name) : QObject(parent)
{
   if (name) setObjectName(name);
}
#else
QAcceptSocketsThread :: QAcceptSocketsThread(QObject * parent, const char * name) : QObject(parent, name)
{
   // empty
}
#endif

QAcceptSocketsThread :: ~QAcceptSocketsThread()
{
   ShutdownInternalThread();  // just in case (note this assumes the user isn't going to subclass this class!)
}

void QAcceptSocketsThread :: SignalOwner()
{
#if QT_VERSION >= 0x040000
   QEvent * evt = newnothrow QEvent((QEvent::Type)QMTT_SIGNAL_EVENT);
#else
   QCustomEvent * evt = newnothrow QCustomEvent(QMTT_SIGNAL_EVENT);
#endif
   if (evt) QCoreApplication::postEvent(this, evt);
       else WARN_OUT_OF_MEMORY;
}

bool QAcceptSocketsThread :: event(QEvent * event)
{
   if (event->type() == QMTT_SIGNAL_EVENT)
   {
      MessageRef next;

      // Check for any new messages from our HTTP thread
      while(GetNextReplyFromInternalThread(next) >= 0)
      {
         switch(next()->what)
         {
            case AST_EVENT_NEW_SOCKET_ACCEPTED:
            {
               RefCountableRef tag;
               if (next()->FindTag(AST_NAME_SOCKET, tag) == B_NO_ERROR)
               {
                  ConstSocketRef sref(tag, false);
                  if (sref()) emit ConnectionAccepted(sref);
               }
            }
            break;
         }
      }
      return true;
   }
   else return QObject::event(event);
}

}; // end namespace muscle
