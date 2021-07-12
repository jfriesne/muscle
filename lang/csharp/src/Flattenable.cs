namespace muscle.support {
  using System.IO;

  /// <summary>
  /// Interface for objects that can be flattened and unflattened 
  /// from Be-style byte streams.
  /// </summary>
  ///    
  public abstract class Flattenable : muscle.support.TypeConstants {
    
    /// <summary>
    /// Should return true iff every object of this type has a 
    /// flattened size that is known at compile time.
    /// </summary>
    public abstract bool isFixedSize();
    
    /// <summary>
    /// Should return the type code identifying this type of object.
    /// </summary>
    public abstract int typeCode();
    
    /// <summary>
    /// Should return the number of bytes needed to store this object 
    /// in its current state.
    /// </summary>
    public abstract int flattenedSize();
    
    /// <summary>
    /// Should return a clone of this object.
    /// </summary>
    public abstract Flattenable cloneFlat();
    
    /// <summary>
    /// Should set this object's state equal to that of (setFromMe), 
    /// or throw an UnflattenFormatException if it can't be done.
    /// </summary>
    /// <param name="setFromMe>the object we want to be like</param>
    /// <exception cref="System.InvalidCastException"/>
    ///
    public abstract void setEqualTo(Flattenable setFromMe);
    
    /// <summary> 
    /// Should store this object's state into (buffer).
    /// </summary>
    /// <param name="writer"/>
    /// <exception cref="IOException"/>
    ///
    public abstract void flatten(BinaryWriter writer);
    
    /// <summary> 
    /// Should return true iff a buffer with type_code (code) can 
    /// be used to reconstruct
    /// this object's state.
    /// </summary>
    /// <param name="code">A type code ant, e.g. B_RAW_TYPE or B_STRING_TYPE, 
    /// or something custom.</param>
    /// <returns>True iff this object can unflatten from a buffer of 
    /// the given type, false otherwise.</returns>
    ///
    public abstract bool allowsTypeCode(int code);
    
    /// <summary> 
    /// Should attempt to restore this object's state from the given buffer.
    /// </summary>
    /// <param name="reader">The stream to read the object from</param>
    /// <param name="numBytes">The number of bytes the object takes up in 
    /// the stream, or negative if this is unknown.</param>
    /// <exception cref="IOException"/>
    /// <exception cref="UnflattenFormatException"/>
    ///
    public abstract void unflatten(BinaryReader reader, int numBytes);
  }   
}

