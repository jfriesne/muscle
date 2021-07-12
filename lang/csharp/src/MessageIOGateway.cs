
namespace muscle.iogateway {
  using muscle.support;
  using muscle.message;

  using System;
  using System.IO;

  /// <summary>
  /// A gateway that converts to and from the 'standard' MUSCLE 
  /// flattened message byte stream.
  /// </summary>

  public class MessageIOGateway : AbstractMessageIOGateway {
    public MessageIOGateway() : 
      this(MUSCLE_MESSAGE_ENCODING_DEFAULT)
    {
 
    }
    
    // <summary>
    // Constructs a MessageIOGateway whose outgoing encoding
    // is one of MUSCLE_MESSAGE_ENCODING_*.
    private MessageIOGateway(int encoding) 
    {
      _encoding = encoding;
    }


    /// <summary>
    /// Reads from the input stream until a Message can be assembled 
    /// and returned.
    /// </summary>
    ///
    /// <param name="reader">the input stream from which to read</param>
    /// <exception cref="IOException"/>
    /// <exception cref="UnflattenFormatException"/>
    /// <returns>The next assembled Message</returns>
    ///
    public Message unflattenMessage(Stream inputStream)
    {
      BinaryReader reader = new BinaryReader(inputStream);
      int numBytes = reader.ReadInt32();
      int encoding = reader.ReadInt32();

      int independent = reader.ReadInt32();
      inputStream.Seek(-4, SeekOrigin.Current);

      Message pmsg = new Message();
      pmsg.unflatten(reader, numBytes);
      
      return pmsg;
    }
    
    /// <summary>
    /// Converts the given Message into bytes and sends it out the stream.
    /// </summary>
    /// <param name="outputStream">the data stream to send the converted bytes
    /// </param>
    /// <param name="msg">the message to convert</param>
    /// <exception cref="IOException"/>
    ///
    public void flattenMessage(Stream outputStream, Message msg)
    {
      int flattenedSize = msg.flattenedSize();

      if (flattenedSize >= 32 &&
	  _encoding >= MUSCLE_MESSAGE_ENCODING_ZLIB_1 &&
	  _encoding <= MUSCLE_MESSAGE_ENCODING_ZLIB_9) {

	// currently do not compress outgoing messages
	// do later
	BinaryWriter writer = new BinaryWriter(outputStream);
	writer.Write((int) flattenedSize);
	writer.Write((int) MessageIOGateway.MUSCLE_MESSAGE_ENCODING_DEFAULT);
	msg.flatten(writer);
	writer.Flush();
      }
      else {
	BinaryWriter writer = new BinaryWriter(outputStream);
	writer.Write((int) flattenedSize);
	writer.Write((int) MessageIOGateway.MUSCLE_MESSAGE_ENCODING_DEFAULT);
	msg.flatten(writer);
	writer.Flush();
      }
    }
    
    private int _encoding;

    public const int MESSAGE_HEADER_SIZE = 8;

    // 'Enc0' -- just plain ol' flattened Message objects, with no 
    // special encoding 
    public const int MUSCLE_MESSAGE_ENCODING_DEFAULT = 1164862256;

    // zlib encoding levels
    public const int MUSCLE_MESSAGE_ENCODING_ZLIB_1 = 1164862257;
    public const int MUSCLE_MESSAGE_ENCODING_ZLIB_2 = 1164862258;
    public const int MUSCLE_MESSAGE_ENCODING_ZLIB_3 = 1164862259;
    public const int MUSCLE_MESSAGE_ENCODING_ZLIB_4 = 1164862260;
    public const int MUSCLE_MESSAGE_ENCODING_ZLIB_5 = 1164862261;
    public const int MUSCLE_MESSAGE_ENCODING_ZLIB_6 = 1164862262;
    public const int MUSCLE_MESSAGE_ENCODING_ZLIB_7 = 1164862263;
    public const int MUSCLE_MESSAGE_ENCODING_ZLIB_8 = 1164862264;
    public const int MUSCLE_MESSAGE_ENCODING_ZLIB_9 = 1164862265;
    public const int MUSCLE_MESSAGE_ENCODING_END_MARKER = 1164862266;

    public const int ZLIB_CODEC_HEADER_INDEPENDENT = 2053925219;
    public const int ZLIB_CODEC_HEADER_DEPENDENT = 2053925218;
  }
}
