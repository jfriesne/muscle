namespace muscle.support {
  using System.IO;
  
  /// <summary>
  /// Exception that is thrown when an unflatten() attempt fails, usually
  /// due to unrecognized data in the input stream, or a type mismatch.
  public class UnflattenFormatException : IOException {
    public UnflattenFormatException() : 
      base("unexpected bytes during Flattenable unflatten") { }
	
    public UnflattenFormatException(string s) : base(s) { }
  }
}
