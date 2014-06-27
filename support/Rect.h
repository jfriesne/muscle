/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

/*******************************************************************************
/
/   File:      Rect.h
/
/   Description:     version of Be's BRect class
/
*******************************************************************************/

#ifndef MuscleRect_h
#define MuscleRect_h

#include "support/Flattenable.h"
#include "support/Point.h"

namespace muscle {

/** A portable version of Be's BRect class. */
class Rect : public Tuple<4,float>, public PseudoFlattenable
{
public:
   /** Default Constructor.  
     * Creates a rectangle with upper left point (0,0), and lower right point (-1,-1).
     * Note that this rectangle has a negative area!  (that is to say, it's imaginary)
     */ 
   Rect() {Set(0.0f,0.0f,-1.0f,-1.0f);}

   /** Constructor where you specify the left, top, right, and bottom coordinates */
   Rect(float l, float t, float r, float b) {Set(l,t,r,b);}

   /** Copy constructor */
   Rect(const Rect & rhs) : Tuple<4,float>(rhs) {/* empty */}

   /** Constructor where you specify the leftTop point and the rightBottom point. */
   Rect(Point leftTop, Point rightBottom) {Set(leftTop.x(), leftTop.y(), rightBottom.x(), rightBottom.y());}

   /** Destructor */
   ~Rect() {/* empty */}

   /** convenience method to get the left edge of this Rect */
   inline float   left()   const {return (*this)[0];}

   /** convenience method to set the left edge of this Rect */
   inline float & left()         {return (*this)[0];}

   /** convenience method to get the top edge of this Rect */
   inline float   top()    const {return (*this)[1];}

   /** convenience method to set the top edge of this Rect */
   inline float & top()          {return (*this)[1];}

   /** convenience method to get the right edge of this Rect */
   inline float   right()  const {return (*this)[2];}

   /** convenience method to set the right edge of this Rect */
   inline float & right()        {return (*this)[2];}

   /** convenience method to get the bottom edge of this Rect */
   inline float   bottom() const {return (*this)[3];}

   /** convenience method to set the bottom edge of this Rect */
   inline float & bottom()       {return (*this)[3];}

   /** Set a new position for the rectangle. */
   inline void Set(float l, float t, float r, float b)
   {
      left()   = l;
      top()    = t;
      right()  = r;
      bottom() = b;
   }

   /** Print debug information about this rectangle to stdout or to a file you specify.
     * @param optFile If non-NULL, the text will be printed to this file.  If left as NULL, stdout will be used as a default.
     */
   void PrintToStream(FILE * optFile = NULL) const
   {
      if (optFile == NULL) optFile = stdout;
      fprintf(optFile, "Rect: leftTop=(%f,%f) rightBottom=(%f,%f)\n", left(), top(), right(), bottom());
    }

   /** Returns the left top corner of the rectangle. */
   inline Point LeftTop() const {return Point(left(), top());}

   /** Returns the right bottom corner of the rectangle. */
   inline Point RightBottom() const {return Point(right(), bottom());}

   /** Returns the left bottom corner of the rectangle. */
   inline Point LeftBottom() const {return Point(left(), bottom());}

   /** Returns the right top corner of the rectangle. */
   inline Point RightTop() const {return Point(right(), top());}

   /** Set the left top corner of the rectangle. */
   inline void SetLeftTop(const Point p) {left() = p.x(); top() = p.y();}

   /** Set the right bottom corner of the rectangle. */
   inline void SetRightBottom(const Point p) {right() = p.x(); bottom() = p.y();}

   /** Set the left bottom corner of the rectangle. */
   inline void SetLeftBottom(const Point p) {left() = p.x(); bottom() = p.y();}

   /** Set the right top corner of the rectangle. */
   inline void SetRightTop(const Point p) {right() = p.x(); top() = p.y();}

   /** Makes the rectangle smaller by the amount specified in both the x and y dimensions */
   inline void InsetBy(Point p) {InsetBy(p.x(), p.y());}

   /** Makes the rectangle smaller by the amount specified in both the x and y dimensions */
   inline void InsetBy(float dx, float dy) {left() += dx; top() += dy; right() -= dx; bottom() -= dy;}

   /** Translates the rectangle by the amount specified in both the x and y dimensions */
   inline void OffsetBy(Point p) {OffsetBy(p.x(), p.y());}

   /** Translates the rectangle by the amount specified in both the x and y dimensions */
   inline void OffsetBy(float dx, float dy) {left() += dx; top() += dy; right() += dx; bottom() += dy;}

   /** Translates the rectangle so that its top left corner is at the point specified. */
   inline void OffsetTo(Point p) {OffsetTo(p.x(), p.y());}

   /** Translates the rectangle so that its top left corner is at the point specified. */
   inline void OffsetTo(float x, float y) {right() = x + Width(); bottom() = y + Height(); left() = x; top() = y;}

   /** If this Rect has negative width or height, modifies it to have positive width and height.   */
   void Rationalize() 
   {
      if (left() > right()) {float t = left(); left() = right(); right() = t;}
      if (top() > bottom()) {float t = top(); top() = bottom(); bottom() = t;}
   }

   /** Returns a rectangle whose area is the intersecting subset of this rectangle's and (r)'s */
   inline Rect operator&(Rect r) const 
   {
      Rect ret(*this);
      if (this != &r)
      {
         if (ret.left()   < r.left())   ret.left()   = r.left();
         if (ret.right()  > r.right())  ret.right()  = r.right();
         if (ret.top()    < r.top())    ret.top()    = r.top();
         if (ret.bottom() > r.bottom()) ret.bottom() = r.bottom();
      }
      return ret;
   }

   /** Returns a rectangle whose area is a superset of the union of this rectangle's and (r)'s */
   inline Rect operator|(Rect r) const 
   {
      Rect ret(*this);
      if (this != &r)
      {
         if (r.left()   < ret.left())   ret.left()   = r.left();
         if (r.right()  > ret.right())  ret.right()  = r.right();
         if (r.top()    < ret.top())    ret.top()    = r.top();
         if (r.bottom() > ret.bottom()) ret.bottom() = r.bottom();
      }
      return ret;
   }

   /** Causes this rectangle to be come the union of itself and (rhs). */
   inline Rect & operator |= (const Rect & rhs) {(*this) = (*this) | rhs; return *this;}

   /** Causes this rectangle to be come the intersection of itself and (rhs). */
   inline Rect & operator &= (const Rect & rhs) {(*this) = (*this) & rhs; return *this;}

   /** Returns true iff this rectangle and (r) overlap in space. */
   inline bool Intersects(Rect r) const {return (r&(*this)).IsValid();}

   /** Returns true iff this rectangle's area is non imaginary (i.e. Width() and Height()) are both non-negative) */
   inline bool IsValid() const {return ((Width() >= 0.0f)&&(Height() >= 0.0f));}

   /** Returns the area of this rectangle. */
   inline float Area() const {return IsValid() ? (Width()*Height()) : 0.0f;}

   /** Returns the width of this rectangle. */
   inline float Width() const {return right() - left();}

   /** Returns the width of this rectangle as an integer. */
   inline int32 IntegerWidth() const {return (int32)ceil(Width());}

   /** Returns the height of this rectangle. */
   inline float Height() const {return bottom()-top();}

   /** Returns the height of this rectangle as an integer. */
   inline int32 IntegerHeight() const {return (int32)ceil(Height());}

   /** Returns true iff this rectangle contains the specified point. */
   inline bool Contains(Point p) const {return ((p.x() >= left())&&(p.x() <= right())&&(p.y() >= top())&&(p.y() <= bottom()));}

   /** Returns true iff this rectangle fully the specified rectangle. */
   inline bool Contains(Rect p) const {return ((Contains(p.LeftTop()))&&(Contains(p.RightTop()))&&(Contains(p.LeftBottom()))&&(Contains(p.RightBottom())));}

   /** Part of the pseudo-Flattenable API:  Returns true. */
   bool IsFixedSize() const {return true;}

   /** Part of the pseudo-Flattenable API:  Returns B_RECT_TYPE. */
   uint32 TypeCode() const {return B_RECT_TYPE;}

   /** Returns true iff (tc) equals B_RECT_TYPE. */
   bool AllowsTypeCode(uint32 tc) const {return (TypeCode()==tc);}

   /** Part of the pseudo-Flattenable API:  Returns 4*sizeof(float). */
   uint32 FlattenedSize() const {return 4*sizeof(float);}

   /** Returns a 32-bit checksum for this object. */
   uint32 CalculateChecksum() const {return CalculateChecksumForFloat(left()) + (3*CalculateChecksumForFloat(top())) + (5*CalculateChecksumForFloat(right())) + (7*CalculateChecksumForFloat(bottom()));}

   /** Flattens this rectangle into an endian-neutral byte buffer.
    *  @param buffer Points to the byte buffer to write into.  There must be at least FlattenedSize() bytes there. 
    */
   void Flatten(uint8 * buffer) const
   {
      float * buf = (float *) buffer;
      uint32 oL = B_HOST_TO_LENDIAN_IFLOAT(left());   muscleCopyOut(&buf[0], oL);
      uint32 oT = B_HOST_TO_LENDIAN_IFLOAT(top());    muscleCopyOut(&buf[1], oT);
      uint32 oR = B_HOST_TO_LENDIAN_IFLOAT(right());  muscleCopyOut(&buf[2], oR);
      uint32 oB = B_HOST_TO_LENDIAN_IFLOAT(bottom()); muscleCopyOut(&buf[3], oB);
   }

   /** Unflattens this rectangle from an endian-neutral byte buffer.
    *  @param buffer Points to the byte buffer to read data from.
    *  @param size The number of bytes available in (buffer).  There should be at least FlattenedSize() bytes there.
    *  @return B_NO_ERROR on success, or B_ERROR if (buffer) was too small.
    */
   status_t Unflatten(const uint8 * buffer, uint32 size)
   {
      if (size >= FlattenedSize())
      {
         float * buf = (float *) buffer;
         uint32 i0; muscleCopyIn(i0, &buf[0]); left()   = B_LENDIAN_TO_HOST_IFLOAT(i0);
         uint32 i1; muscleCopyIn(i1, &buf[1]); top()    = B_LENDIAN_TO_HOST_IFLOAT(i1);
         uint32 i2; muscleCopyIn(i2, &buf[2]); right()  = B_LENDIAN_TO_HOST_IFLOAT(i2);
         uint32 i3; muscleCopyIn(i3, &buf[3]); bottom() = B_LENDIAN_TO_HOST_IFLOAT(i3);
         return B_NO_ERROR;
      }
      else return B_ERROR;
   }

   /** This is implemented so that if Rect is used as the key in a Hashtable, the Tuple HashCode() method will be 
     * selected by the AutoChooseHashFunctor template logic, instead of the PODHashFunctor.  (Unfortunately 
     * AutoChooseHashFunctor doesn't check the superclasses when it looks for a HashCode method)
     */
   uint32 HashCode() const {return Tuple<4,float>::HashCode();}
};

DECLARE_ALL_TUPLE_OPERATORS(Rect,float);

}; // end namespace muscle

#endif 
