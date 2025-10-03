/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MusclePlainTextMessageIOGateway_h
#define MusclePlainTextMessageIOGateway_h

#include "iogateway/AbstractMessageIOGateway.h"

namespace muscle {

/** The 'what' code that will be found in incoming Messages. */
#define PR_COMMAND_TEXT_STRINGS 1886681204  // 'ptxt'

/** This is the name of the string field used to hold text lines. */
#define PR_NAME_TEXT_LINE "tl"

/**
 * This gateway translates lines of text (separated by "\r", "\n", or "\r\n") into
 * Messages.  It can be used for "telnet-style" network interactions, where each command (or reply)
 * is represented by a line of ASCII/UTF8 text.
 * Incoming and outgoing messages may have one or more strings in their PR_NAME_TEXT_LINE field.
 * Each of these strings represents a line of text (line-separator chars not included)
 */
class PlainTextMessageIOGateway : public AbstractMessageIOGateway
{
public:
   /** Default constructor */
   PlainTextMessageIOGateway();

   /** Destructor */
   virtual ~PlainTextMessageIOGateway();

   MUSCLE_NODISCARD virtual bool HasBytesToOutput() const;
   virtual void Reset();

   /** Set the end-of-line string to be attached to outgoing text lines.
    *  @param str New end-of-line string to use.  (Default value is "\r\n")
    */
   virtual void SetOutgoingEndOfLineString(const char * str) {_eolString = str;}

   /** If set to true, then any "leftover" text after the last carriage return
    *  will be added to the incoming Message.  If false (the default), then incoming
    *  text without a carriage return will be buffered internally until the next
    *  carriage return is received.
    *  @param f true to flush partial incoming lines; false to wait until they are fully received
    */
   void SetFlushPartialIncomingLines(bool f) {_flushPartialIncomingLines = f;}

   /** Returns the flush-partial-incoming-lines value, as set by SetFlushPartialIncomingLines(). */
   MUSCLE_NODISCARD bool GetFlushPartialIncomingLines() const {return _flushPartialIncomingLines;}

   /** Force any pending input to be immediately flushed out
     * @param receiver The object to call MessageReceivedFromGateway() on, if necessary.
     */
   void FlushInput(AbstractGatewayMessageReceiver & receiver);

   /** Returns true iff we currently have any incoming text buffered up waiting for a carriage return to be received */
   MUSCLE_NODISCARD bool HasBufferedIncomingText() const {return _incomingText.HasChars();}

protected:
   virtual io_status_t DoOutputImplementation(uint32 maxBytes = MUSCLE_NO_LIMIT);
   virtual io_status_t DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes = MUSCLE_NO_LIMIT);

   /** Called after each block of data is read from the IO device.  Default implementation
     * is a no-op.  A subclass may override this to modify the input data, if necessary.
     * @param buf a pointer to the just-read data, to be inspected and/or modified.
     * @param bufLen current number of valid bytes in the buffer.  May be modified by method code.
     * @param maxLen The total size of (buf).  (bufLen) must not be set greater than this value.
     */
   virtual void FilterInputBuffer(char * buf, uint32 & bufLen, uint32 maxLen);

private:
   io_status_t DoOutputImplementationAux(uint32 maxBytes, uint32 recurseDepth);
   MessageRef AddIncomingText(const MessageRef & msg, const char * s);

   MessageRef _currentSendingMessage;
   String _currentSendText;
   String _eolString;
   int32 _currentSendLineIndex;
   int32 _currentSendOffset;
   bool _prevCharWasCarriageReturn;
   String _incomingText;
   bool _flushPartialIncomingLines;

   DECLARE_COUNTED_OBJECT(PlainTextMessageIOGateway);
};
DECLARE_REFTYPES(PlainTextMessageIOGateway);

/** This class is the same as a PlainTextMessageIOGateway, except that
  * some filtering logic has been added to strip out telnet control codes.
  * This class is useful when accepting TCP connections from telnet clients.
  */
class TelnetPlainTextMessageIOGateway : public PlainTextMessageIOGateway
{
public:
   /** Default constructor */
   TelnetPlainTextMessageIOGateway();

   /** Destructor */
   virtual ~TelnetPlainTextMessageIOGateway();

protected:
   virtual void FilterInputBuffer(char * buf, uint32 & bufLen, uint32 maxLen);

private:
   bool _inSubnegotiation;
   int _commandBytesLeft;
};
DECLARE_REFTYPES(TelnetPlainTextMessageIOGateway);

} // end namespace muscle

#endif
