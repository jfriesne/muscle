
namespace muscle.support {
  using System.IO;

  public class Point : Flattenable {
    /// Our data members, exposed for convenience
    public float x = 0.0f;
    public float y = 0.0f;
    
    /// Default constructor, sets the point to be (0, 0) */
    public Point() { x = 0.0f; y = 0.0f; }
    
    public Point(float X, float Y) {
      set(X, Y);
    }
    
    public override Flattenable cloneFlat() {
      return new Point(x, y);
    }
    
    public override void setEqualTo(Flattenable setFromMe)
    {
      Point p = (Point) setFromMe; 
      set(p.x, p.y);
    }
    
    public void set(float X, float Y) {
      x = X; y = Y;
    }
    
    /// <summary>
    /// If the point is outside the rectangle specified by the two arguments,
    /// it will be moved horizontally and/or vertically until it falls 
    /// inside the rectangle.
    /// </summary>
    /// <param name="topLeft">Minimum values acceptable for X and Y</param>
    /// <param name="bottomRight">Maximum values acceptable for X and Y</param>
    ///
    public void constrainTo(Point topLeft, Point bottomRight)
    {
      if (x < topLeft.x) 
	x = topLeft.x;
      if (y < topLeft.y) 
	y = topLeft.y;
      if (x > bottomRight.x) 
	x = bottomRight.x;
      if (y > bottomRight.y) 
	y = bottomRight.y;
    }
    
    public override string ToString() {
      return "Point: " + x + " " + y;
    }
    
    public Point add(Point rhs) {
      return new Point(x+rhs.x, y+rhs.y);
    }
    
    public Point subtract(Point rhs) {
      return new Point(x-rhs.x, y-rhs.y);
    }
    
    public void addToThis(Point p)
    {
      x += p.x;
      y += p.y;
    }
    
    public void subtractFromThis(Point p)
    {
      x -= p.x;
      y -= p.y;
    }
    
    public override bool Equals(object o)
    {
      if (o is Point) {
	Point p = (Point) o;
	return ((x == p.x)&&(y == p.y));
      }

      return false;
    }

    public override int GetHashCode()
    {
      return this.ToString().GetHashCode();
    }
    
    /// <summary>
    /// Returns true.
    /// </summary>
    public override bool isFixedSize() {return true;}
    
    /// <summary>
    /// Returns B_POINT_TYPE.
    /// </summary>
    public override int typeCode() {
      return TypeConstants.B_POINT_TYPE;
    }
    
    /// <summary>
    /// Returns 8 (2*sizeof(float))
    /// </summary>
    public override int flattenedSize() {
      return 8;
    }
    
    public override void flatten(BinaryWriter writer)
    {
      writer.Write((float) x);
      writer.Write((float) y);
    }

    /// <summary> 
    /// Returns true iff (code) is B_POINT_TYPE.
    /// </summary>
    public override bool allowsTypeCode(int code) {
      return (code == B_POINT_TYPE);
    }
    
    public override void unflatten(BinaryReader reader, int numBytes)
    {
      x = reader.ReadSingle();
      y = reader.ReadSingle();
    }
  }
}
