/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef QMuscleSupport_h
#define QMuscleSupport_h

#include <QString>
#include <QPoint>
#include <QRect>
#include <QSize>

#include "support/MuscleSupport.h"  // for PODHashFunctor, etc

#if QT_VERSION >= 0x040000
# include <QHash>
#else
# include "util/String.h"
#endif

namespace muscle {

/** Enables the use of QStrings as keys in a MUSCLE Hashtable. */
template <> class PODHashFunctor<QString>
{
public:
   /** Returns a hash code for the given QString object.
     * @param str The QString to calculate a hash code for.
     */
   MUSCLE_NODISCARD uint32 operator () (const QString & str) const
   {
#if QT_VERSION >= 0x040000
      return (uint32) qHash(str);
#else
      QByteArray ba = str.utf8();  // Yes, in Qt 3.x it's called utf8(), not toUtf8()
      return muscle::CalculateHashCode(ba.data(), ba.size());
#endif
   }

   /** Returns true iff the two QStrings are equal.
     * @param k1 first key to compare
     * @param k2 second key to compare
     */
   MUSCLE_NODISCARD bool AreKeysEqual(const QString & k1, const QString & k2) const {return (k1==k2);}
};

/** Enables the use of QSizes as keys in a MUSCLE Hashtable. */
template <> class PODHashFunctor<QSize>
{
public:
   /** Returns a hash code for the given QSize object.
     * @param sz The QSize to calculate a hash code for.
     */
   MUSCLE_NODISCARD uint32 operator () (const QSize & sz) const
   {
      const uint32 hw = CalculateHashCode(sz.width());
      const uint32 hh = CalculateHashCode(sz.height());
      return (hw + (hh*13));
   }

   /** Returns true iff the two QSize are equal.
     * @param k1 first key to compare
     * @param k2 second key to compare
     */
   MUSCLE_NODISCARD bool AreKeysEqual(const QSize & k1, const QSize & k2) const {return (k1==k2);}
};

/** Enables the use of QPoints as keys in a MUSCLE Hashtable. */
template <> class PODHashFunctor<QPoint>
{
public:
   /** Returns a hash code for the given QPoint object.
     * @param pt The QPoint to calculate a hash code for.
     */
   MUSCLE_NODISCARD uint32 operator () (const QPoint & pt) const
   {
      const uint32 hx = CalculateHashCode(pt.x());
      const uint32 hy = CalculateHashCode(pt.y());
      return (hx + (hy*13));
   }

   /** Returns true iff the two QPoint are equal.
     * @param k1 first key to compare
     * @param k2 second key to compare
     */
   MUSCLE_NODISCARD bool AreKeysEqual(const QPoint & k1, const QPoint & k2) const {return (k1==k2);}
};

/** Enables the use of QRects as keys in a MUSCLE Hashtable. */
template <> class PODHashFunctor<QRect>
{
public:
   /** Returns a hash code for the given QRect object.
     * @param r The QRect to calculate a hash code for.
     */
   MUSCLE_NODISCARD uint32 operator () (const QRect & r) const
   {
      const uint32 hl = CalculateHashCode(r.left());
      const uint32 ht = CalculateHashCode(r.top());
      const uint32 hw = CalculateHashCode(r.width());
      const uint32 hh = CalculateHashCode(r.height());
      return (hl + (ht*13) + (hw*39) + (hh*103));
   }

   /** Returns true iff the two QRect are equal.
     * @param k1 first key to compare
     * @param k2 second key to compare
     */
   MUSCLE_NODISCARD bool AreKeysEqual(const QRect & k1, const QRect & k2) const {return (k1==k2);}
};

}  // end namespace muscle

#endif
