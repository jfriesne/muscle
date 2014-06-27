import zlib
import struct
import cStringIO

import message

ZLIB_CODEC_HEADER_INDEPENDENT = 2053925219  # 'zlic'
MUSCLE_ZLIB_FIELD_NAME_STRING = "_zlib"
MUSCLE_ZLIB_HEADER_SIZE       = (2*4)

def DeflateMessage(msg, compressionLevel=6, force=1):
   """ Given a Message object, returns an equivalent Message object with all the data compressed.
       The returned Messages's what-code will be the same as the original Message.
       If the passed-in Message is already compressed, it will be returned unaltered.
       @param msg The Message to compress
       @param compressionLevel Optional zlib compression-level.  May be between 0 and 9; default value is 6.
       @param force If set to 0, compression will not occur if the compressed Message turns out to be larger
                    than the original Message.  (instead the original Message will be returned).  Defaults to 1.
   """
   if (msg.GetFieldItem(MUSCLE_ZLIB_FIELD_NAME_STRING, message.B_RAW_TYPE) == None):
      defMsg = message.Message(msg.what)
      origFlatSize = msg.FlattenedSize()
      headerBuf = cStringIO.StringIO()
      headerBuf.write(struct.pack("<2L", ZLIB_CODEC_HEADER_INDEPENDENT, origFlatSize))
      bodyBuf = cStringIO.StringIO()
      msg.Flatten(bodyBuf)
      cobj = zlib.compressobj(compressionLevel)
      cdata = cobj.compress(bodyBuf.getvalue())
      cdata = cdata + cobj.flush(zlib.Z_SYNC_FLUSH)
      defMsg.PutFieldContents(MUSCLE_ZLIB_FIELD_NAME_STRING, message.B_RAW_TYPE, headerBuf.getvalue()+cdata)
      if (force == 1) or (defMsg.FlattenedSize() < origFlatSize):
         return defMsg
   return msg

def InflateMessage(msg):
   """ If the passed-in Message object was compressed with DeflateMessage(), this method will
       do the inverse operation:  It will decompress it and return the original, uncompressed Message.
       Otherwise, the passed-in Message will be returned.
       @param msg The Message to decompress, if it is compressed.
   """
   compStr = msg.GetFieldItem(MUSCLE_ZLIB_FIELD_NAME_STRING, message.B_RAW_TYPE)
   if compStr != None and len(compStr) >= MUSCLE_ZLIB_HEADER_SIZE:
      header, rawLen = struct.unpack("<2L", compStr[0:MUSCLE_ZLIB_HEADER_SIZE])
      if header == ZLIB_CODEC_HEADER_INDEPENDENT:
         infMsg = message.Message()
         dobj = zlib.decompressobj()
         idata = dobj.decompress(compStr[MUSCLE_ZLIB_HEADER_SIZE:], rawLen)
         idata = idata + dobj.flush()
         infMsg.Unflatten(cStringIO.StringIO(idata))
         return infMsg
   return msg

# unit test
if __name__ == "__main__":
   msg = message.Message(12345)
   msg.PutString("A String", "Yes it is")
   msg.PutInt32("A Number", 666)
   msg.PutFloat("Some Floats", [1.0, 2.2, 3.3, 4.4, 5.5, 6.6])
   msg.PutInt8("Some Ints", [1,1,1,1,1,1,1,1,1,1,1,1,1])
   msg.PutInt8("More Ints", [1,1,1,1,1,1,1,1,1,1,1,1,1])
   msg.PutInt8("Yet More Ints", [1,1,1,1,1,1,1,1,1,1,1,1,1])
   msg.PutInt8("Still More Ints", [1,1,1,1,1,1,1,1,1,1,1,1,1])
   print "---------------------\nOriginal flatSize=", msg.FlattenedSize()
   msg.PrintToStream()

   msg = DeflateMessage(msg)
   print "---------------------\nDeflated flatSize=", msg.FlattenedSize()
   msg.PrintToStream()
    
   msg = InflateMessage(msg)
   print "---------------------\nReinflated flatSize=", msg.FlattenedSize()
   msg.PrintToStream()
