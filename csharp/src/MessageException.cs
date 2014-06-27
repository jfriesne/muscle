namespace muscle.message {

  /// <summary>
  /// Base class for the various Exceptions that the Message class may throw.
  /// </summary>
  public class MessageException : System.Exception {
    public MessageException(string s) : base(s) { }
      
    public MessageException() : 
      base("Error accessing a Message field") { }
  }
}
