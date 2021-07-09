#ifndef QSocketCallbackMechanism_h
#define QSocketCallbackMechanism_h

#include <QSocketNotifier>
#include "util/SocketCallbackMechanism.h"

namespace muscle {

/** This class implements the ICallbackMechanism interface
  * in a Qt-specific way that makes it easy to integrate
  * callbacks into a Qt-based GUI program.
  */
class QSocketCallbackMechanism : public QObject, public SocketCallbackMechanism
{
Q_OBJECT

public:
   /** Constructor
     * @param optParent passed to the QObject constructor.
     */
   QSocketCallbackMechanism(QObject * optParent = NULL)
      : QObject(optParent)
      , _notifier(GetDispatchThreadNotifierSocket().GetFileDescriptor(), QSocketNotifier::Read, optParent)
   {
      QObject::connect(&_notifier, SIGNAL(activated(int)), SLOT(NotifierActivated()));
   }

   /** Destructor */
   virtual ~QSocketCallbackMechanism()
   {
      _notifier.setEnabled(false);
   }

private slots:
   void NotifierActivated() {DispatchCallbacks();}

private:
   QSocketNotifier _notifier;
};

};  // end muscle namespace

#endif
