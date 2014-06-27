/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.micromuscle.support;
import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;

/** A Java equivalent to Be's BPoint class */
public class Point implements Flattenable
{
   /** Our data members, exposed for convenience */   
   public float x = 0.0f;
   public float y = 0.0f;

   /** Default constructor, sets the point to be (0, 0) */
   public Point() { /* empty */ }

   /** Constructor where you specify the initial value of the point 
    *  @param X Initial X position
    *  @param Y Initial Y position
    */
   public Point(float X, float Y) {set(X, Y);}

   /** Returns a clone of this object. */
   public Flattenable cloneFlat() {return new Point(x, y);}

   /** Should set this object's state equal to that of (setFromMe), or throw an UnflattenFormatException if it can't be done.
    *  @param setFromMe The object we want to be like.
    *  @throws ClassCastException if (setFromMe) isn't a Point
    */
   public void setEqualTo(Flattenable setFromMe) throws ClassCastException
   {
      Point p = (Point) setFromMe; 
      set(p.x, p.y);
   }
   
   /** Sets a new value for the point.
    *  @param X The new X value 
    *  @param Y The new Y value 
    */
   public void set(float X, float Y) {x = X; y = Y;}

   /** If the point is outside the rectangle specified by the two arguments,
    *  it will be moved horizontally and/or vertically until it falls inside the rectangle.
    *  @param topLeft Minimum values acceptable for X and Y
    *  @param bottomRight Maximum values acceptable for X and Y
    */
   public void constrainTo(Point topLeft, Point bottomRight)
   {
      if (x < topLeft.x) x = topLeft.x;
      if (y < topLeft.y) y = topLeft.y;
      if (x > bottomRight.x) x = bottomRight.x;
      if (y > bottomRight.y) y = bottomRight.y;
   }

   public String toString() {return "Point: " + x + " " + y;}

   /** Returns another Point whose values are the sum of this point's values and (p)'s values. 
    *  @param rhs Point to add
    *  @return Resulting point
    */
   public Point add(Point rhs) {return new Point(x+rhs.x, y+rhs.y);}

   /** Returns another Point whose values are the difference of this point's values and (p)'s values. 
    *  @param rhs Point to subtract from this
    *  @return Resulting point
    */
   public Point subtract(Point rhs) {return new Point(x-rhs.x, y-rhs.y);}

   /** Adds (p)'s values to this point's values */
   public void addToThis(Point p)
   {
      x += p.x;
      y += p.y;
   }

   /** Subtracts (p)'s values from this point's values */
   public void subtractFromThis(Point p)
   {
      x -= p.x;
      y -= p.y;
   }

   /** Returns true iff (p)'s values match this point's values */
   public boolean equals(Object o)
   {
      if (o instanceof Point)
      {
         Point p = (Point) o;
         return ((x == p.x)&&(y == p.y));
      }
      return false;
   }

   /** Returns true. */
   public boolean isFixedSize() {return true;}
   
   /** Returns B_POINT_TYPE. */
   public int typeCode() {return TypeConstants.B_POINT_TYPE;}
   
   /** Returns 8 (2*sizeof(float)) */
   public int flattenedSize() {return 8;}
   
   /** 
    *  Should store this object's state into (buffer). 
    *  @param out The DataOutput to send the data to.
    *  @throws IOException if there is a problem writing out the output bytes.
    */
   public void flatten(DataOutput out) throws IOException
   {
      out.writeFloat(x);
      out.writeFloat(y);
   }
   
   /** 
    *  Returns true iff (code) is B_POINT_TYPE.
    */
   public boolean allowsTypeCode(int code) {return (code == B_POINT_TYPE);}
   
   /** 
    *  Should attempt to restore this object's state from the given buffer.
    *  @param in The stream to read the object from.
    *  @throws IOException if there is a problem reading in the input bytes.
    *  @throws UnflattenFormatException if the bytes that were read in weren't in the expected format.
    */
   public void unflatten(DataInput in, int numBytes) throws IOException, UnflattenFormatException
   {
      x = in.readFloat();
      y = in.readFloat();
   }
   
   public int hashCode() {return Float.floatToIntBits(x) + Float.floatToIntBits(y);}
}

