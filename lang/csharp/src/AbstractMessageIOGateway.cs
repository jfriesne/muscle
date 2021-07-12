
namespace muscle.iogateway {
  using muscle.support;
  using muscle.message;

  using System.IO;

  /// <summary>
  /// Interface for an object that knows how to translate bytes
  /// into Messages, and vice versa.
  /// </summary>
  ///
  public interface AbstractMessageIOGateway {

    /// <summary>
    /// Reads from the input stream until a Message can be assembled and 
    /// returned.
    /// </summary>
    /// <param name="inputStream">The input stream from which to read.</param>
    /// <exception cref="IOException"/>
    /// <exception cref="UnflattenFormatException"/>
    /// <returns>The next assembled Message.</returns>

    Message unflattenMessage(Stream inputStream);
    
    /// <summary>
    /// Converts the given Message into bytes and sends it out the stream.
    /// </summary>
    ///
    /// <param name="outputStream"/>
    /// <param name="msg"/>
    /// <exception cref="IOException"/>

    void flattenMessage(Stream outputStream, Message msg);
  }
}
