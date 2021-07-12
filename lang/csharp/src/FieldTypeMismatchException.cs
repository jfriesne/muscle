namespace muscle.message {
  /// <summary>
  /// Exception that is thrown if you try to access a field in a Message
  /// by the wrong type (e.g. calling getInt() on a string field or somesuch)
  /// </summary>

  public class FieldTypeMismatchException : MessageException {
    public FieldTypeMismatchException(string s) : base(s) { }
	
    public FieldTypeMismatchException() : 
      base("Message entry type mismatch") { }
  }
}
