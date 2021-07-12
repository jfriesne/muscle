/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.txt file for details. */
package com.meyer.muscle.support;

import java.awt.Rectangle;
import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;
import java.nio.ByteBuffer;

/** A Java equivalent of Be's BRect class. */
public class Rect implements Flattenable
{
   /** Exposed member variable representing the position of the left edge of the rectangle. */
   public float left = 0.0f;

   /** Exposed member variable representing the position of the top edge of the rectangle. */
   public float top = 0.0f;

   /** Exposed member variable representing the position of the right edge of the rectangle. */
   public float right = -1.0f;

   /** Exposed member variable representing the position of the bottom edge of the rectangle. */
   public float bottom = -1.0f;

   /** Default Constructor.  
     * Creates a rectangle with upper left point (0,0), and lower right point (-1,-1).
     * Note that this rectangle has a negative area!  (that is, it's imaginary)
     */ 
   public Rect() { /* empty */ }

   /** Constructor where you specify the left, top, right, and bottom coordinates */
   public Rect(float l, float t, float r, float b) {set(l,t,r,b);}

   /** Copy constructor */
   public Rect(Rect rhs) {setEqualTo(rhs);}

   /** Constructor where you specify the leftTop point and the rightBottom point. */
   public Rect(Point leftTop, Point rightBottom) {set(leftTop.x, leftTop.y, rightBottom.x, rightBottom.y);}

   /** Returns a clone of this object. */
   public Flattenable cloneFlat() {return new Rect(left, top, right, bottom);}

   /** Sets this equal to (rhs).
    *  @param rhs The rectangle to become like.
    *  @throws ClassCastException if (rhs) isn't a Rect
    */
   public void setEqualTo(Flattenable rhs)
   {
      Rect copyMe = (Rect) rhs;
      set(copyMe.left, copyMe.top, copyMe.right, copyMe.bottom);
   } 

   /** Set a new position for the rectangle. */
   public void set(float l, float t, float r, float b)
   {
      left   = l;
      top    = t;
      right  = r;
      bottom = b;
   }

   /** Print the rectangle's current state to (out) */
   public String toString()
   {
      return "Rect: leftTop=(" + left + "," + top + ") rightBottom=(" + right + "," + bottom + ")";
   }

   /** Returns the left top corner of the rectangle. */
   public Point leftTop() {return new Point(left, top);}

   /** Returns the right bottom corner of the rectangle. */
   public Point rightBottom() {return new Point(right, bottom);}

   /** Returns the left bottom corner of the rectangle. */
   public Point leftBottom() {return new Point(left, bottom);}

   /** Returns the right top corner of the rectangle. */
   public Point rightTop() {return new Point(right, top);}

   /** Set the left top corner of the rectangle. */
   public void setLeftTop(Point p) {left = p.x; top = p.y;}

   /** Set the right bottom corner of the rectangle. */
   public void setRightBottom(Point p) {right = p.x; bottom = p.y;}

   /** Set the left bottom corner of the rectangle. */
   public void setLeftBottom(Point p) {left = p.x; bottom = p.y;}

   /** Set the right top corner of the rectangle. */
   public void setRightTop(Point p) {right = p.x; top = p.y;}

   /** Makes the rectangle smaller by the amount specified in both the x and y dimensions */
   public void insetBy(Point p) {insetBy(p.x, p.y);}

   /** Makes the rectangle smaller by the amount specified in both the x and y dimensions */
   public void insetBy(float dx, float dy) {left += dx; top += dy; right -= dx; bottom -= dy;}

   /** Translates the rectangle by the amount specified in both the x and y dimensions */
   public void offsetBy(Point p) {offsetBy(p.x, p.y);}

   /** Translates the rectangle by the amount specified in both the x and y dimensions */
   public void offsetBy(float dx, float dy) {left += dx; top += dy; right += dx; bottom += dy;}

   /** Translates the rectangle so that its top left corner is at the point specified. */
   public void offsetTo(Point p) {offsetTo(p.x, p.y);}

   /** Translates the rectangle so that its top left corner is at the point specified. */
   public void offsetTo(float x, float y) {right = x + width(); bottom = y + height(); left = x; top = y;}

   /** Comparison Operator.  Returns true iff (r)'s dimensions are exactly the same as this rectangle's. */
   public boolean equals(Object o) 
   {
      if (o instanceof Rect)
      {
         Rect r = (Rect) o;
         return (left == r.left)&&(top == r.top)&&(right == r.right)&&(bottom == r.bottom);
      }
      return false;
   }

   /** Returns a rectangle whose area is the intersecting subset of this rectangle's and (r)'s */
   public Rect intersect(Rect r) 
   {
      Rect ret = new Rect(this);
      if (ret.left   < r.left)   ret.left   = r.left;
      if (ret.right  > r.right)  ret.right  = r.right;
      if (ret.top    < r.top)    ret.top    = r.top;
      if (ret.bottom > r.bottom) ret.bottom = r.bottom;
      return ret;
   }

   /** Returns a rectangle whose area is a superset of the union of this rectangle's and (r)'s */
   public Rect unify(Rect r) 
   {
      Rect ret = new Rect(this);
      if (r.left   < ret.left)   ret.left   = r.left;
      if (r.right  > ret.right)  ret.right  = r.right;
      if (r.top    < ret.top)    ret.top    = r.top;
      if (r.bottom > ret.bottom) ret.bottom = r.bottom;
      return ret;
   }

   /** Returns true iff this rectangle and (r) overlap in space. */
   public boolean intersects(Rect r) {return (r.intersect(this).isValid());}

   /** Returns true iff this rectangle's area is non imaginary (ie width() and height()) are both non-negative) */
   public boolean isValid() {return ((width() >= 0.0f)&&(height() >= 0.0f));}

   /** Returns the width of this rectangle. */
   public float width() {return right - left;}

   /** Returns the height of this rectangle. */
   public float height() {return bottom - top;}

   /** Returns true iff this rectangle contains the specified point. */
   public boolean contains(Point p) 
   {
      return ((p.x >= left)&&(p.x <= right)&&(p.y >= top)&&(p.y <= bottom));
   }

   /** Returns true iff this rectangle fully the specified rectangle. */
   public boolean contains(Rect p) 
   {
      return ((contains(p.leftTop()))&&(contains(p.rightTop()))&&
              (contains(p.leftBottom()))&&(contains(p.rightBottom())));
   }

   /** Part of the Flattenable API:  Returns true. */
   public boolean isFixedSize() {return true;}

   /** Part of the Flattenable API:  Returns B_RECT_TYPE. */
   public int typeCode() {return B_RECT_TYPE;}

   /** Part of the Flattenable API:  Returns 12 (ie 4*sizeof(float)). */
   public int flattenedSize() {return 16;}

   /** Returns true only if code is B_RECT_TYPE. */
   public boolean allowsTypeCode(int code) {return (code == B_RECT_TYPE);}

   /**
    *  Should store this object's state into (buffer).
    *  @param out The DataOutput to send the data to.
    *  @throws IOException if there is a problem writing out the output bytes.
    */
   public void flatten(DataOutput out) throws IOException  
   {
      out.writeFloat(left);
      out.writeFloat(top);
      out.writeFloat(right);   
      out.writeFloat(bottom);
   }

   public void flatten(ByteBuffer out) throws IOException  
   {
      out.putFloat(left);
      out.putFloat(top);
      out.putFloat(right);   
      out.putFloat(bottom);
   }

   /**
    *  Should attempt to restore this object's state from the given buffer.
    *  @param in The stream to read the object from.
    *  @throws IOException if there is a problem reading in the input bytes.
    *  @throws UnflattenFormatException if the bytes that were read in weren't in the expected format.
    */
   public void unflatten(DataInput in, int numBytes) throws IOException, UnflattenFormatException
   {
      left   = in.readFloat();
      top    = in.readFloat();
      right  = in.readFloat();
      bottom = in.readFloat();
   }

   public void unflatten(ByteBuffer in, int numBytes) throws IOException, UnflattenFormatException
   {
      left   = in.getFloat();
      top    = in.getFloat();
      right  = in.getFloat();
      bottom = in.getFloat();
   }

   /**
    * Creates a Java Rectangle from the current values.
    * @return a java.awt.Rectangle representing the same area.
    */
   public Rectangle getRectangle() {
      return new Rectangle((int)left, (int)top, (int)right, (int)bottom);
   }
}
