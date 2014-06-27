/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef QMuscleSupport_h
#define QMuscleSupport_h

#include "support/MuscleSupport.h"  // for PODHashFunctor, etc

#if QT_VERSION >= 0x040000
# include <qhash.h>
#else
# include "util/String.h"
#endif

namespace muscle {

/** Enables the use of QStrings as keys in a MUSCLE Hashtable. */
template <>
class PODHashFunctor<QString>
{
public:
   /** Returns a hash code for the given QString object.
     * @param str The QString to calculate a hash code for.
     */
   uint32 operator () (const QString & str) const 
   {
#if QT_VERSION >= 0x040000
      return qHash(str);
#else
      QByteArray ba = str.utf8();  // Yes, in Qt 3.x it's called utf8(), not toUtf8()
      return muscle::CalculateHashCode(ba.data(), ba.size());
#endif
   }

   /** Returns true iff the two QStrings are equal. */
   bool AreKeysEqual(const QString & k1, const QString & k2) const {return (k1==k2);}
};

};  // end namespace muscle

#endif
