/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MusclePoint_h
#define MusclePoint_h

/*******************************************************************************
/
/   File:      Point.h
/
/   Description:     version of Be's Point class
/
*******************************************************************************/

#include <math.h>  // for sqrt()
#include "support/PseudoFlattenable.h"
#include "support/Tuple.h"

namespace muscle {

/*----------------------------------------------------------------------*/
/*----- Point class --------------------------------------------*/

/** A portable version of Be's BPoint class. */
class MUSCLE_NODISCARD Point MUSCLE_FINAL_CLASS : public Tuple<2,float>, public PseudoFlattenable<Point>
{
public:
   /** Default constructor, sets the point to be (0.0f, 0.0f) */
   MUSCLE_CONSTEXPR_17 Point() {/* empty */}

   /** Constructor where you specify the initial value of the point
    *  @param ax Initial x position
    *  @param ay Initial y position
    */
   MUSCLE_CONSTEXPR_17 Point(float ax, float ay) {Set(ax, ay);}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   MUSCLE_CONSTEXPR_17 Point(const Point & rhs) : Tuple<2,float>(rhs) {/* empty */}

   /** Convenience method to get the x value of this Point */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline float GetX() const {return (*this)[0];}

   /** Synonym for GetX() */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline float x() const {return GetX();}

   /** Sets a new value for our X-coordinate.
     * @param x the new X value
     */
   MUSCLE_CONSTEXPR_17 void SetX(float x) {(*this)[0] = x;}

   /** Convenience method to get the y value of this Point */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline float GetY() const {return (*this)[1];}

   /** Synonym for GetY() */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline float y() const {return GetY();}

   /** Sets a new value for our Y-coordinate.
     * @param y the new Y value
     */
   MUSCLE_CONSTEXPR_17 void SetY(float y) {(*this)[1] = y;}

   /** Sets a new value for the point.
    *  @param ax The new x value
    *  @param ay The new y value
    */
   MUSCLE_CONSTEXPR_17 void Set(float ax, float ay) {SetX(ax); SetY(ay);}

   /** If the point is outside the rectangle specified by the two arguments,
    *  it will be moved horizontally and/or vertically until it falls inside the rectangle.
    *  @param topLeft Minimum values acceptable for x and y
    *  @param bottomRight Maximum values acceptable for x and y
    */
   void ConstrainTo(Point topLeft, Point bottomRight)
   {
      if (x() < topLeft.x())     SetX(topLeft.x());
      if (y() < topLeft.y())     SetY(topLeft.y());
      if (x() > bottomRight.x()) SetX(bottomRight.x());
      if (y() > bottomRight.y()) SetY(bottomRight.y());
   }

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   inline Point & operator = (const Point & rhs) {Set(rhs.x(), rhs.y()); return *this;}

   /** Print debug information about the point to stdout or to a file you specify.
     * @param optFile If non-NULL, the text will be printed to this file.  If left as NULL, stdout will be used as a default.
     */
   void PrintToStream(FILE * optFile = NULL) const
   {
      if (optFile == NULL) optFile = stdout;
      fprintf(optFile, "Point: %f %f\n", x(), y());
   }

   /** Part of the PseudoFlattenable pseudo-interface:  Returns true */
   MUSCLE_NODISCARD bool IsFixedSize() const {return true;}

   /** Part of the PseudoFlattenable pseudo-interface:  Returns B_POINT_TYPE */
   MUSCLE_NODISCARD uint32 TypeCode() const {return B_POINT_TYPE;}

   /** Part of the PseudoFlattenable pseudo-interface:  Returns 2*sizeof(float) */
   MUSCLE_NODISCARD static uint32 FlattenedSize() {return GetNumItemsInTuple()*sizeof(float);}

   /** @copydoc DoxyTemplate::CalculateChecksum() const */
   MUSCLE_NODISCARD uint32 CalculateChecksum() const {return CalculateChecksumForFloat(x()) + (3*CalculateChecksumForFloat(y()));}

   /** @copydoc DoxyTemplate::Flatten(DataFlattener) const */
   void Flatten(DataFlattener flat) const {flat.WriteFloats(GetItemPointer(0), GetNumItemsInTuple());}

   /** @copydoc DoxyTemplate::Unflatten(DataUnflattener &) */
   status_t Unflatten(DataUnflattener & unflat) {return unflat.ReadFloats(GetItemPointer(0), GetNumItemsInTuple());}

   /** This is implemented so that if Rect is used as the key in a Hashtable, the Tuple HashCode() method will be
     * selected by the AutoChooseHashFunctor template logic, instead of the PODHashFunctor.  (Unfortunately
     * AutoChooseHashFunctor doesn't check the superclasses when it looks for a HashCode method)
     */
   MUSCLE_NODISCARD uint32 HashCode() const {return Tuple<2,float>::HashCode();}

   /** Returns the distance between this point and (pt).
     * @param pt The point we want to calculate the distance to.
     * @returns a non-negative distance value.
     */
   MUSCLE_NODISCARD float GetDistanceTo(const Point & pt) const {return (float)sqrt(GetDistanceToSquared(pt));}

   /** Returns the square of the distance between this point and (pt).
     * @param pt The point we want to calculate the distance to.
     * @returns a non-negative distance-squared value.
     * @note this method is more efficient that calling GetDistanceTo(), since it doesn't have to call sqrt().
     */
   MUSCLE_NODISCARD float GetDistanceToSquared(const Point & pt) const
   {
      const float dx = pt.x()-x();
      const float dy = pt.y()-y();
      return ((dx*dx)+(dy*dy));
   }
};
DECLARE_ALL_TUPLE_OPERATORS(Point,float);

} // end namespace muscle

#endif
