namespace muscle.support {
  using System.IO;
  
  public class Rect : Flattenable {
    public float left = 0.0f;
    public float top = 0.0f;
    public float right = -1.0f;
    public float bottom = -1.0f;
    
    /// <summary>
    /// Creates a rectangle with upper left point (0,0), and 
    /// lower right point (-1,-1).
    /// Note that this rectangle has a negative area!
    /// (that is, it's imaginary)
    /// </summary>
    public Rect() { /* empty */ }	
    
    public Rect(float l, float t, float r, float b) {set(l,t,r,b);}
    
    public Rect(Rect rhs) {setEqualTo(rhs);}
    
    public Rect(Point leftTop, Point rightBottom) 
    {
      set(leftTop.x, leftTop.y, rightBottom.x, rightBottom.y);
    }
    
    public override Flattenable cloneFlat() {
      return new Rect(left, top, right, bottom);
    }
    
    public override void setEqualTo(Flattenable rhs)
    {
      Rect copyMe = (Rect) rhs;
      set(copyMe.left, copyMe.top, copyMe.right, copyMe.bottom);
    } 
    
    public void set(float l, float t, float r, float b)
    {
      left   = l;
      top    = t;
      right  = r;
      bottom = b;
    }
    
    public override string ToString()
    {
      return "Rect: leftTop=(" + left + "," + top + ") rightBottom=(" + right + "," + bottom + ")";
    }
    

    public Point leftTop() 
    {
      return new Point(left, top);
    }
    
    public Point rightBottom() 
    {
      return new Point(right, bottom);
    }
    
    public Point leftBottom() 
    {
      return new Point(left, bottom);
    }
    
    public Point rightTop() 
    {
      return new Point(right, top);
    }
    
    public void setLeftTop(Point p) 
    {
      left = p.x; top = p.y;
    }
    
    public void setRightBottom(Point p) 
    {
      right = p.x; bottom = p.y;
    }
    
    public void setLeftBottom(Point p) 
    {
      left = p.x; bottom = p.y;
    }
    
    public void setRightTop(Point p) 
    {
      right = p.x; top = p.y;
    }
    
    /// <summary>
    /// Makes the rectangle smaller by the amount specified in both 
    /// the x and y dimensions
    /// </summary>
    public void insetBy(Point p) {insetBy(p.x, p.y);}
    
    /// <summary>
    /// Makes the rectangle smaller by the amount specified in 
    /// both the x and y dimensions
    /// </summary>
    public void insetBy(float dx, float dy) 
    {
      left += dx; 
      top += dy; 
      right -= dx; 
      bottom -= dy;
    }
    
    /// <summary>
    /// Translates the rectangle by the amount specified in both the x and y 
    /// dimensions
    /// </summary>
    public void offsetBy(Point p) {offsetBy(p.x, p.y);}
    
    /// <summary>
    /// Translates the rectangle by the amount specified in both 
    /// the x and y dimensions
    /// </summary>
    public void offsetBy(float dx, float dy) 
    {
      left += dx; 
      top += dy; 
      right += dx; 
      bottom += dy;
    }
    
    /// <summary>
    /// Translates the rectangle so that its top left corner is at the 
    /// point specified.
    /// </summary>
    public void offsetTo(Point p) 
    {
      offsetTo(p.x, p.y);
    }
    
    /// <summary>
    /// Translates the rectangle so that its top left corner is at 
    /// the point specified.
    /// </summary>
    public void offsetTo(float x, float y) 
    {
      right = x + width(); 
      bottom = y + height(); 
      left = x; top = y;
    }
    
    /// <summary>
    /// Comparison Operator.  Returns true iff (r)'s dimensions are 
    /// exactly the same as this rectangle's.
    /// </summary>
    public override bool Equals(object o) 
    {
      if (o is Rect) {
	Rect r = (Rect) o;
	return (left == r.left) && (top == r.top)
	  && (right == r.right) && (bottom == r.bottom);
      }
      
      return false;
    }
    
    public override int GetHashCode()
    {
      return this.ToString().GetHashCode();
    }
    
    /// <summary>
    /// Returns a rectangle whose area is the intersecting subset of 
    /// this rectangle's and (r)'s
    /// </summary>
    public Rect intersect(Rect r) 
    {
      Rect ret = new Rect(this);
      if (ret.left < r.left)
	ret.left = r.left;
      if (ret.right > r.right)
	ret.right = r.right;
      if (ret.top < r.top)
	ret.top = r.top;
      if (ret.bottom > r.bottom) 
	ret.bottom = r.bottom;
      return ret;
    }
    
    /// <summary> 
    /// Returns a rectangle whose area is a superset of the union of 
    /// this rectangle's and (r)'s
    /// </summary>
    public Rect unify(Rect r) 
    {
      Rect ret = new Rect(this);

      if (r.left < ret.left)   
	ret.left = r.left;
      if (r.right > ret.right)
	ret.right = r.right;
      if (r.top < ret.top)
	ret.top  = r.top;
      if (r.bottom > ret.bottom) 
	ret.bottom = r.bottom;

      return ret;
    }
	
    public bool intersects(Rect r) 
    {
      return (r.intersect(this).isValid());
    }
    
    public bool isValid() 
    {
      return ((width() >= 0.0f)&&(height() >= 0.0f));
    }
    
    public float width() 
    {
      return right - left;
    }
    
    public float height() 
    {
      return bottom - top;
    }
    
    public bool contains(Point p) 
    {
      return ((p.x >= left)&&(p.x <= right)&&(p.y >= top)&&(p.y <= bottom));
    }
    
    public bool contains(Rect p) 
    {
      return ((contains(p.leftTop()))&&(contains(p.rightTop()))&&
	      (contains(p.leftBottom()))&&(contains(p.rightBottom())));
    }
    
    public override bool isFixedSize() 
    {
      return true;
    }
    
    public override int typeCode() 
    {
      return B_RECT_TYPE;
    }
    
    public override int flattenedSize() 
    {
      return 16;
    }
    
    /// <summary>
    /// Returns true only if code is B_RECT_TYPE.
    /// </summary>
    public override bool allowsTypeCode(int code) 
    {
      return (code == B_RECT_TYPE);
    }
    
    public override void flatten(BinaryWriter writer)  
    {
      writer.Write(left);
      writer.Write(top);
      writer.Write(right);   
      writer.Write(bottom);
    }
    
    public override void unflatten(BinaryReader reader, int numBytes) {
      left   = reader.ReadSingle();
      top    = reader.ReadSingle();
      right  = reader.ReadSingle();
      bottom = reader.ReadSingle();
    }  
  }
}
