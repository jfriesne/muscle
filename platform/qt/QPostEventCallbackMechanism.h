#ifndef QPostEventCallbackMechanism_h
#define QPostEventCallbackMechanism_h

#include <QCoreApplication>
#include <QEvent>

#include "util/ICallbackMechanism.h"

namespace muscle {

/** This class implements the ICallbackMechanism interface by calling qApp->postEvent() */
class QPostEventCallbackMechanism : public QObject, public ICallbackMechanism
{
public:
   /** Constructor
     * @param optParent passed to the QObject constructor.
     */
   QPostEventCallbackMechanism(QObject * optParent = NULL)
      : QObject(optParent)
   {
      // empty
   }

protected:
   virtual void SignalDispatchThread() {qApp->postEvent(this, new QEvent((QEvent::Type)CALLBACKMECHANISM_EVENT_CODE));}

   virtual bool event(QEvent * e)
   {
      if (e->type() == CALLBACKMECHANISM_EVENT_CODE)
      {
         DispatchCallbacks();
         return true;
      }
      else return QObject::event(e);
   }

private:
   enum {CALLBACKMECHANISM_EVENT_CODE = QEvent::User+55555};
};

};  // end muscle namespace

#endif
