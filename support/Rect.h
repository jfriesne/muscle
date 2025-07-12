/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleRect_h
#define MuscleRect_h

/*******************************************************************************
/
/   File:      Rect.h
/
/   Description:     version of Be's BRect class
/
*******************************************************************************/

#include "support/PseudoFlattenable.h"
#include "support/Point.h"
#include "util/OutputPrinter.h"

namespace muscle {

/** A portable version of Be's BRect class. */
class MUSCLE_NODISCARD Rect MUSCLE_FINAL_CLASS : public Tuple<4,float>, public PseudoFlattenable<Rect>
{
public:
   /** Default Constructor.
     * Creates a rectangle with upper left point (0,0), and lower right point (-1,-1).
     * Note that this rectangle has a negative area!  (that is to say, it's imaginary)
     */
   MUSCLE_CONSTEXPR_17 Rect() {Set(0.0f,0.0f,-1.0f,-1.0f);}

   /** Constructor where you specify the left, top, right, and bottom coordinates
     * @param l the left-edge X coordinate
     * @param t the top-edge Y coordinate
     * @param r the right-edge X coordinate
     * @param b the bottom-edge Y coordinate
     */
   MUSCLE_CONSTEXPR_17 Rect(float l, float t, float r, float b) {Set(l,t,r,b);}

   /** Constructor where you specify the leftTop point and the rightBottom point.
     * @param leftTop a Point to indicate the left/top corner of this Rect
     * @param rightBottom a Point to indicate the right/bottom corner of this Rect
     */
   MUSCLE_CONSTEXPR_17 Rect(Point leftTop, Point rightBottom) {Set(leftTop.x(), leftTop.y(), rightBottom.x(), rightBottom.y());}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   MUSCLE_CONSTEXPR_17 Rect(const Rect & rhs) : Tuple<4,float>(rhs) {/* empty */}

   /** Returns the X coordinate of the left edge of this Rect */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline float GetLeft() const {return (*this)[0];}

   /** Synonym for GetLeft() */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline float left() const {return GetLeft();}

   /** Sets the X coordinate of the left edge of this Rect
     * @param x new coordinate value for the left edge
     */
   MUSCLE_CONSTEXPR_17 void SetLeft(float x) {(*this)[0] = x;}

   /** Returns the Y coordinate of the top edge of this Rect */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline float GetTop() const {return (*this)[1];}

   /** Synonym for GetTop() */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline float top() const {return GetTop();}

   /** Sets the Y coordinate of the top edge of this Rect
     * @param y new coordinate value for the top edge
     */
   MUSCLE_CONSTEXPR_17 void SetTop(float y) {(*this)[1] = y;}

   /** Returns the X coordinate of the right edge of this Rect */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline float GetRight() const {return (*this)[2];}

   /** Synonym for GetRight() */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline float right() const {return GetRight();}

   /** Sets the X coordinate of the right edge of this Rect
     * @param x new coordinate value for the right edge
     */
   MUSCLE_CONSTEXPR_17 void SetRight(float x) {(*this)[2] = x;}

   /** Returns the Y coordinate of the bottom edge of this Rect */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline float GetBottom() const {return (*this)[3];}

   /** Synonym for GetBottom() */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline float bottom() const {return GetBottom();}

   /** Sets the Y position of the bottom edge of this Rect
     * @param y new coordinate value for the bottom edge
     */
   MUSCLE_CONSTEXPR_17 void SetBottom(float y) {(*this)[3] = y;}

   /** Set a new position for the rectangle.
     * @param l the new left-edge coordinate
     * @param t the new top-edge coordinate
     * @param r the new right-edge coordinate
     * @param b the new bottom-edge coordinate
     */
   MUSCLE_CONSTEXPR_17 void Set(float l, float t, float r, float b)
   {
      SetLeft(l);
      SetTop(t);
      SetRight(r);
      SetBottom(b);
   }

   /** @copydoc DoxyTemplate::Print(const OutputPrinter &) const */
   void Print(const OutputPrinter & p) const {p.printf("Rect: leftTop=(%f,%f) rightBottom=(%f,%f)\n", left(), top(), right(), bottom());}

   /** Returns the left top corner of the rectangle. */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline Point LeftTop() const {return Point(left(), top());}

   /** Returns the right bottom corner of the rectangle. */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline Point RightBottom() const {return Point(right(), bottom());}

   /** Returns the left bottom corner of the rectangle. */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline Point LeftBottom() const {return Point(left(), bottom());}

   /** Returns the right top corner of the rectangle. */
   MUSCLE_NODISCARD MUSCLE_CONSTEXPR_17 inline Point RightTop() const {return Point(right(), top());}

   /** Set the left top corner of the rectangle.
     * @param p the new left/top corner for this Rect
     */
   inline MUSCLE_CONSTEXPR_17 void SetLeftTop(const Point & p) {SetLeft(p.x()); SetTop(p.y());}

   /** Set the right bottom corner of the rectangle.
     * @param p the new right/bottom corner for this Rect
     */
   inline MUSCLE_CONSTEXPR_17 void SetRightBottom(const Point & p) {SetRight(p.x()); SetBottom(p.y());}

   /** Set the left bottom corner of the rectangle.
     * @param p the new left/bottom corner for this Rect
     */
   inline MUSCLE_CONSTEXPR_17 void SetLeftBottom(const Point & p) {SetLeft(p.x()); SetBottom(p.y());}

   /** Set the right top corner of the rectangle.
     * @param p the new right/top corner for this Rect
     */
   inline MUSCLE_CONSTEXPR_17 void SetRightTop(const Point & p) {SetRight(p.x()); SetTop(p.y());}

   /** Makes the rectangle smaller by the amount specified in both the x and y dimensions
     * @param p a Point whose dimensions indicate how much smaller to make our x and y dimensions on each edge, respectively
     */
   inline void InsetBy(const Point & p) {InsetBy(p.x(), p.y());}

   /** Makes the rectangle smaller by the amount specified in both the x and y dimensions
     * @param dx the number of pixels right to move the left edge; and the number of pixels left to move the right edge
     * @param dy the number of pixels down to move the top edge; and the number of pixels up to move the bottom edge
     */
   inline void InsetBy(float dx, float dy) {SetLeft(left()+dx); SetTop(top()+dy); SetRight(right()-dx); SetBottom(bottom()-dy);}

   /** Translates the rectangle by the amount specified in both the x and y dimensions
     * @param p a Point whose dimensions indicate how far to translate this Rect in each direction
     */
   inline void OffsetBy(const Point & p) {OffsetBy(p.x(), p.y());}

   /** Returns the point at the center of this Rect */
   inline Point GetCenter() const {return Point((left()+right())/2.0f, (top()+bottom())/2.0f);}

   /** Translates the rectangle so that the rectangle's center will be at the given location.
     * @param cx how far to the right to move our left and right edges
     * @param cy how far down to move our top and bottom edges
     */
   inline void CenterTo(float cx, float cy)
   {
      const float w2 = GetWidth()/2.0f;
      SetLeft( cx-w2);
      SetRight(cx+w2);

      const float h2 = GetHeight()/2.0f;
      SetTop(   cy-h2);
      SetBottom(cy+h2);
   }

   /** Translates the rectangle by the amount specified in both the x and y dimensions
     * @param dx how far to the right to move our left and right edges
     * @param dy how far down to move our top and bottom edges
     */
   inline void OffsetBy(float dx, float dy) {SetLeft(left()+dx); SetTop(top()+dy); SetRight(right()+dx); SetBottom(bottom()+dy);}

   /** Translates the rectangle so that its top left corner is at the point specified.
     * @param p the new upper-left corner for this rectangle
     */
   inline void OffsetTo(const Point & p) {OffsetTo(p.x(), p.y());}

   /** Translates the rectangle so that its top left corner is at the point specified.
     * @param x the new left edge for this Rectangle
     * @param y the new top edge for this Rectangle
     */
   inline void OffsetTo(float x, float y) {SetRight(x+GetWidth()); SetBottom(y + GetHeight()); SetLeft(x); SetTop(y);}

   /** If this Rect has negative width or height, modifies it to have positive width and height.   */
   void Rationalize()
   {
      if (left() > right()) {const float t = left(); SetLeft(right()); SetRight(t);}
      if (top() > bottom()) {const float t = top();  SetTop(bottom()); SetBottom(t);}
   }

   /** Returns true iff this Rect's height and width are both non-negative */
   bool IsRational() const {return ((GetWidth()>=0.0f)&&(GetHeight()>=0.0f));}

   /** Convenience method:  Returns an irrational Rectangle (with negative width and height) to represent "no area" */
   static Rect GetIrrationalRect() {return Rect(0.0f, 0.0f, -1.0f, -1.0f);}

   /** Returns a rectangle whose area is the intersecting subset of this rectangle's and (r)'s
     * @param r the Rect to intersect with this rectangle
     */
   inline Rect operator&(const Rect & r) const
   {
      if ((IsRational() == false)||(r.IsRational() == false)) return GetIrrationalRect();

      Rect ret;
      ret.SetLeft(  muscleMax(left(),   r.left()));
      ret.SetTop(   muscleMax(top(),    r.top()));
      ret.SetRight( muscleMin(right(),  r.right()));
      ret.SetBottom(muscleMin(bottom(), r.bottom()));
      return ret.IsRational() ? ret : Rect();
   }

   /** Returns a rectangle whose area is a superset of the union of this rectangle's and (r)'s
     * @param r the Rect to unify with this rectangle
     */
   inline Rect operator|(const Rect & r) const
   {
      if (IsRational()   == false) return r;
      if (r.IsRational() == false) return *this;

      Rect ret(*this);
      if (this != &r)
      {
         if (r.left()   < ret.left())   ret.SetLeft(  r.left());
         if (r.right()  > ret.right())  ret.SetRight( r.right());
         if (r.top()    < ret.top())    ret.SetTop(   r.top());
         if (r.bottom() > ret.bottom()) ret.SetBottom(r.bottom());
      }
      return ret;
   }

   /** Returns a rectangle whose area is a superset of the union of this rectangle's and (p)'s
     * @param p the Point to unify with this rectangle
     */
   inline Rect operator|(const Point & p) const {return (*this | Rect(p.x(), p.y(), p.x(), p.y()));}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   inline Rect & operator = (const Rect & rhs) {Set(rhs.left(), rhs.top(), rhs.right(), rhs.bottom()); return *this;}

   /** Causes this rectangle to be come the union of itself and (rhs).
     * @param rhs the rectangle to unify with this one
     */
   inline Rect & operator |= (const Rect & rhs) {(*this) = (*this) | rhs; return *this;}

   /** Causes this rectangle to be come the union of itself and (rhs).
     * @param rhs the point to unify with this one
     */
   inline Rect & operator |= (const Point & rhs) {(*this) = (*this) | rhs; return *this;}

   /** Causes this rectangle to be come the intersection of itself and (rhs).
     * @param rhs the rectangle to intersect with this one
     */
   inline Rect & operator &= (const Rect & rhs) {(*this) = (*this) & rhs; return *this;}

   /** Returns true iff this rectangle and (r) overlap in space.
     * @param r the Rect to check to see if it intersects with this Rect
     */
   MUSCLE_NODISCARD inline bool Intersects(const Rect & r) const {return (r&(*this)).IsValid();}

   /** Returns true iff this rectangle's area is non imaginary (ie GetWidth() and GetHeight()) are both non-negative) */
   MUSCLE_NODISCARD inline bool IsValid() const {return ((GetWidth() >= 0.0f)&&(GetHeight() >= 0.0f));}

   /** Returns the area of this rectangle. */
   MUSCLE_NODISCARD inline float Area() const {return IsValid() ? (GetWidth()*GetHeight()) : 0.0f;}

   /** Returns the width of this rectangle. */
   MUSCLE_NODISCARD inline float GetWidth() const {return right() - left();}

   /** Returns the width of this rectangle, rounded up to the nearest integer. */
   MUSCLE_NODISCARD inline int32 GetWidthAsInteger() const {return (int32)ceil(GetWidth());}

   /** Returns the height of this rectangle. */
   MUSCLE_NODISCARD inline float GetHeight() const {return bottom()-top();}

   /** Returns the height of this rectangle, rounded up to the nearest integer. */
   MUSCLE_NODISCARD inline int32 GetHeightAsInteger() const {return (int32)ceil(GetHeight());}

   /** Returns true iff this rectangle contains the specified point.
     * @param x x coordinate of the point
     * @param y y coordinate of the point
     */
   MUSCLE_NODISCARD inline bool Contains(float x, float y) const {return ((x >= left())&&(x <= right())&&(y >= top())&&(y <= bottom()));}

   /** Returns true iff this rectangle contains the specified point.
     * @param p the Point to check to see if it falls within this Rectangle's area
     */
   MUSCLE_NODISCARD inline bool Contains(const Point & p) const {return ((p.x() >= left())&&(p.x() <= right())&&(p.y() >= top())&&(p.y() <= bottom()));}

   /** Returns true iff this rectangle fully encompasses the specified rectangle.
     * @param p the Rect to check to see if it's entirely inside this Rect.
     */
   MUSCLE_NODISCARD inline bool Contains(Rect p) const {return ((Contains(p.LeftTop()))&&(Contains(p.RightTop()))&&(Contains(p.LeftBottom()))&&(Contains(p.RightBottom())));}

   /** Convenience method:  Returns the smallest Rectangle that contains all the Points in the passed-in array
     * @param points Pointer to an array of Points to calculate the bounding box of
     * @param numPoints the number of Points that (points) points to
     * @note if (numPoints) is zero, an irrational Rect will be returned.
     */
   static Rect GetBoundingBox(const Point * points, uint32 numPoints)
   {
      if (numPoints == 0) return GetIrrationalRect();  // no points returns irrational Rectangle

      Rect r(points[0], points[0]);
      for (uint32 i=1; i<numPoints; i++) r |= points[i];  // yes, deliberately starting at 1
      return r;
   }

   /** Convenience method:  Returns the smallest Rectangle that contains all the Rects in the passed-in array
     * @param rects Pointer to an array of Rects to calculate the bounding box of
     * @param numRects the number of Rects that (rects) points to
     */
   static Rect GetBoundingBox(const Rect * rects, uint32 numRects) {Rect r; for (uint32 i=0; i<numRects; i++) r |= rects[i]; return r;}

   /** Part of the pseudo-Flattenable API:  Returns true. */
   MUSCLE_NODISCARD bool IsFixedSize() const {return true;}

   /** Part of the pseudo-Flattenable API:  Returns B_RECT_TYPE. */
   MUSCLE_NODISCARD uint32 TypeCode() const {return B_RECT_TYPE;}

   /** Part of the PseudoFlattenable API:  Returns 4*sizeof(float). */
   MUSCLE_NODISCARD static uint32 FlattenedSize() {return GetNumItemsInTuple()*sizeof(float);}

   /** @copydoc DoxyTemplate::CalculateChecksum() const */
   MUSCLE_NODISCARD uint32 CalculateChecksum() const {return CalculatePODChecksum(left()) + (3*CalculatePODChecksum(top())) + (5*CalculatePODChecksum(right())) + (7*CalculatePODChecksum(bottom()));}

   /** @copydoc DoxyTemplate::Flatten(DataFlattener) const */
   void Flatten(DataFlattener flat) const {flat.WriteFloats(GetItemPointer(0), GetNumItemsInTuple());}

   /** @copydoc DoxyTemplate::Unflatten(DataUnflattener &) */
   status_t Unflatten(DataUnflattener & unflat) {return unflat.ReadFloats(GetItemPointer(0), GetNumItemsInTuple());}

   /** This is implemented so that if Rect is used as the key in a Hashtable, the Tuple HashCode() method will be
     * selected by the AutoChooseHashFunctor template logic, instead of the PODHashFunctor.  (Unfortunately
     * AutoChooseHashFunctor doesn't check the superclasses when it looks for a HashCode method)
     */
   MUSCLE_NODISCARD uint32 HashCode() const {return Tuple<4,float>::HashCode();}
};

DECLARE_ALL_TUPLE_OPERATORS(Rect,float);

} // end namespace muscle

#endif
