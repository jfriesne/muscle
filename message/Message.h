/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleMessage_h
#define MuscleMessage_h

/******************************************************************************
/
/   File:       Message.h
/
/   Description:   OS-independent version of Be's BMessage class.
/         Doesn't contain all of BMessage's functionality,
/         but keeps enough of it to be useful as a portable
/         data object (dictionary) in cross-platform apps.
/         Also adds some functionality that BMessage doesn't have,
/         such as iterators and optional direct (non-copying) access to internal
/         data (for efficiency)
/
*******************************************************************************/

#include "message/MessageImpl.h" // this is the only place that MessageImpl.h should ever be #included!

namespace muscle {

class Message;
DECLARE_REFTYPES(Message);

/** Returns a reference to an empty/default Message object.
  * Useful in cases where you need to refer to an empty Message but don't wish to be
  * constantly constructing/destroying temporary Message objects.
  */
inline const Message & GetEmptyMessage() {return GetDefaultObjectForType<Message>();}

/** Same as GetEmptyMessage(), except it returns a ConstMessageRef instead of a Message. */
const ConstMessageRef & GetEmptyMessageRef();

/** This function returns a pointer to a singleton ObjectPool that can be
 *  used to minimize the number of Message allocations and deletions by
 *  recycling the Message objects.
 */
MessageRef::ItemPool * GetMessagePool();

/** Convenience method:  Gets a Message from the Message pool and returns a reference to it.
 *  @param what The 'what' code to set in the returned Message.
 *  @return Reference to a Message object, or a NULL ref on failure (out of memory).
 */
MessageRef GetMessageFromPool(uint32 what = 0L);

/** As above, except that the Message is obtained from the specified pool instead of from the default Message pool.
 *  @param pool the ObjectPool to allocate the Message from.
 *  @param what The 'what' code to set in the returned Message.
 *  @return Reference to a Message object, or a NULL ref on failure (out of memory).
 */
MessageRef GetMessageFromPool(ObjectPool<Message> & pool, uint32 what = 0L);

/** Convenience method:  Gets a Message from the message pool, makes it equal to (copyMe), and returns a reference to it.
 *  @param copyMe A Message to clone.
 *  @return Reference to a Message object, or a NULL ref on failure (out of memory).
 */
MessageRef GetMessageFromPool(const Message & copyMe);

/** As above, except that the Message is obtained from the specified pool instead of from the default Message pool.
 *  @param pool the ObjectPool to allocate the Message from.
 *  @param copyMe A Message to clone.
 *  @return Reference to a Message object, or a NULL ref on failure (out of memory).
 */
MessageRef GetMessageFromPool(ObjectPool<Message> & pool, const Message & copyMe);

/** Convenience method:  Gets a Message from the message pool, populates it using the flattened Message
 *  bytes at (flatBytes), and returns it.
 *  @param flatBytes The flattened Message bytes (as previously produced by Message::Flatten()) to unflatten from
 *  @param numBytes The number of bytes that (flatBytes) points to.
 *  @return Reference to a Message object, or a NULL ref on failure (out of memory or unflattening error)
 */
MessageRef GetMessageFromPool(const uint8 * flatBytes, uint32 numBytes);

/** As above, except that the Message is obtained from the specified pool instead of from the default Message pool.
 *  @param pool the ObjectPool to allocate the Message from.
 *  @param flatBytes The flattened Message bytes (as previously produced by Message::Flatten()) to unflatten from
 *  @param numBytes The number of bytes that (flatBytes) points to.
 *  @return Reference to a Message object, or a NULL ref on failure (out of memory or unflattening error)
 */
MessageRef GetMessageFromPool(ObjectPool<Message> & pool, const uint8 * flatBytes, uint32 numBytes);

/** Convenience method:  Gets a Message from the message pool, makes it a lightweight copy of (copyMe),
 *  and returns a reference to it.  See Message::MakeLightweightCopyOf() for details.
 *  @param copyMe A Message to make a lightweight copy of.
 *  @return Reference to a Message object, or a NULL ref on failure (out of memory).
 */
MessageRef GetLightweightCopyOfMessageFromPool(const Message & copyMe);

/** As above, except that the Message is obtained from the specified pool instead of from the default Message pool.
 *  @param pool the ObjectPool to allocate the Message from.
 *  @param copyMe A Message to make a lightweight copy of.
 *  @return Reference to a Message object, or a NULL ref on failure (out of memory).
 */
MessageRef GetLightweightCopyOfMessageFromPool(ObjectPool<Message> & pool, const Message & copyMe);

/** This is an iterator that allows you to efficiently iterate over the field names in a Message. */
class MessageFieldNameIterator MUSCLE_FINAL_CLASS
{
public:
   /** Default constructor.
    *  Creates an "empty" iterator;  Use the assignment operator to turn the iterator into something useful.
    */
   MessageFieldNameIterator() : _typeCode(B_ANY_TYPE) {/* empty */}

   /** This form of the constructor creates an iterator that will iterate over field names in the given Message.
     * @param msg the Message whose field names we want to iterate over
     * @param type Type of fields you wish to iterate over, or B_ANY_TYPE to iterate over all fields.
     * @param flags Bit-chord of HTIT_FLAG_* flags you want to use to affect the iteration behaviour.
     *              This bit-chord will get passed on to the underlying HashtableIterator.  Defaults
     *              to zero, which provides the default behaviour.
     */
   MessageFieldNameIterator(const Message & msg, uint32 type = B_ANY_TYPE, uint32 flags = 0);

   /** Destructor. */
   ~MessageFieldNameIterator() {/* empty */}

   /** Moves this iterator forward by one in the field-names list.  */
   void operator++(int) {_iter++; if (_typeCode != B_ANY_TYPE) SkipNonMatchingFieldNames();}

   /** Moves this iterator backward by one in the field-names list.  */
   void operator--(int) {bool b = _iter.IsBackwards(); _iter.SetBackwards(!b); (*this)++; _iter.SetBackwards(b);}

   /** Returns true iff this iterator is pointing at valid field name data.
     * It's only safe to call GetFieldName() if this method returns true.
     */
   bool HasData() const {return _iter.HasData();}

   /** Returns a reference to the current field name.  Note that it is only safe
     * to call this method if HasData() returned true!  The value returned by
     * HasData() may change if the Message we are iterating over is modified.
     */
   const String & GetFieldName() const {return _iter.GetKey();}

   /** Returns the type code (B_STRING_TYPE, B_INT32_TYPE, etc) of iterator's
     * current Message-field (as specified by the return value of GetFieldName()).
     */
   uint32 GetFieldType() const {return _iter.GetValue().TypeCode();}

private:
   friend class Message;
   MessageFieldNameIterator(const HashtableIterator<String, muscle_private::MessageField> & iter, uint32 tc) : _typeCode(tc), _iter(iter) {if (_typeCode != B_ANY_TYPE) SkipNonMatchingFieldNames();}
   void SkipNonMatchingFieldNames();

   uint32 _typeCode;
   HashtableIterator<String, muscle_private::MessageField> _iter;
};

// Version number of the Message serialization protocol.
// Will be the first four bytes of each serialized Message buffer.
// Only buffers beginning with version numbers between these two
// constants (inclusive) will be Unflattened as Messages.

/** The oldest version of the MUSCLE serialization protocol that we still support.  Currently there is only a single protocol version, so this here primarily for future use. */
#define OLDEST_SUPPORTED_PROTOCOL_VERSION 1347235888 // 'PM00'

/** The current version of the MUSCLE serialization protocol that we generate.  Currently there is only a single protocol version, so this here primarily for future use. */
#define CURRENT_PROTOCOL_VERSION          1347235888 // 'PM00'

/**
 *  The Message class implements a serializable container for named, typed data.
 *
 *  Messages are the foundation of the MUSCLE data-exchange protocol, and
 *  they are used to send data between computers, processes, and threads
 *  in a standardized, type-safe, expressive, endian-neutral, and
 *  language-neutral manner.  A Message object can be serialized into
 *  a series of bytes on one machine by calling Flatten() on it, and
 *  after those bytes have been sent across the network, the same Message
 *  can be recreated on the destination machine by calling Unflatten().
 *
 *  This enables efficient and reliable transmission of arbitrarily complex structured data.
 *  A flattened Message can be as small as 12 bytes, or as large as 4 gigabytes.
 *
 *  Each Message object has a single 32-bit integer 'what code', which
 *  can be set to any 32-bit value (often a PR_COMMAND_* or PR_RESULT_*
 *  value, as enumerated in StorageReflectConstants.h)
 *
 *  Each Message can also hold a number of data fields.  Each data field
 *  in a Message is assigned a name string (which also can be arbitrary,
 *  although various strings may have particular meanings in various contexts),
 *  and has a specified data type.  The field names are used as keys to lookup
 *  their associated data, so each field in the Message must have a name that is
 *  unique relative to the other fields in the same Message.  (Note: to make
 *  MUSCLE code more readable and less susceptible to typos, the field name strings
 *  that MUSCLE's PR_COMMAND_* Messages recognize are encoded as PR_NAME_* defines
 *  in StorageReflectConstants.h)
 *
 *  Each data field in a Message holds an array of one or more data items of the
 *  field's specified type, stored (internally) as a double-ended Queue.
 *
 *  The Message class has built-in support for fields containing the following data types:
 *    - Boolean: via AddBool(), FindBool(), GetBool(), etc.
 *    - Double-precision IEEE754 floating point: via AddDouble(), FindDouble(), GetDouble(), etc.
 *    - Single-precision IEEE754 floating point: via AddFloat(), FindFloat(), GetFloat(), etc.
 *    - 64-bit signed integer: via AddInt64(), FindInt64(), GetInt64(), etc.
 *    - 32-bit signed integer: via AddInt32(), FindInt32(), GetInt32(), etc.
 *    - 16-bit signed integer: via AddInt16(), FindInt16(), GetInt16(), etc.
 *    - 8-bit  signed integer: via AddInt8(), FindInt8(), GetInt8(), etc.
 *    - String: (String objects are NUL-terminated character strings):  via AddString(), FindString(), GetString(), etc.
 *    - Message/MessageRef: via AddMessage(), FindMessage(), GetMessage(), etc.
 *    - Raw Byte Buffer: (Variable-length sequences of unstructured uint8 bytes):  via AddData(), FindData(), etc.
 *    - Point: (x and y float values):  via AddPoint(), FindPoint(), GetPoint(), etc.
 *    - Rectangle: (left, top, right, and bottom float values):  via AddRect(), FindRect(), GetRect(), etc.
 *    - Pointer: (Note: Pointer fields do not get serialized):  via AddPointer(), FindPointer(), GetPointer(), etc.
 *
 *  Note that the for support fields of type Message means that Messages can be nested hierarchically;
 *  A single Message object can contain a field of type Message, each of whose elements is another
 *  Message, each of which can contain its own Message fields, and so on.  This allows for very
 *  complex data structures:  for example, Meyer Sound's CueStation software encodes an entire
 *  saved project file in the form of a single serialized Message object.
 *
 *  In addition to the built-in types listed above, it is possible to add fields
 *  containing user-defined objects of arbitary type.  In order to be added to a Message,
 *  a non-built-in object must derive from Flattenable (if you wish to add it to the Message
 *  in its flattened-bytes form, and later on restore it via FindFlat()), or alternatively
 *  the object can be derived from FlatCountable, in which case you can add a reference to
 *  your object (again via AddFlat()) to the Message, and the Message will hold on to your
 *  object directly rather than to a buffer of its flattened bytes.  This can be more efficient,
 *  since when done this way the object need not be serialized and later reconstructed, unless/until
 *  the Message itself needs to be serialized and reconstructed (eg in order to
 *  send it across the network to another machine).
 *
 *  Lastly, it is possible to store inside a Message references to objects of any class that derives
 *  from RefCountable, via the AddTag() method (etc).  Objects added via AddTag() will not be serialized
 *  when the Message is serialized, however, so this method is useful mainly for intra-process communication
 *  or in-RAM data storage.
 *
 *  Note that for quick debugging purposes, it is possible to dump a Message's contents to stdout
 *  at any time by calling PrintToStream() on the Message.
 */
class Message MUSCLE_FINAL_CLASS : public FlatCountable, public Cloneable
{
public:
   /** 32 bit what code, for quick identification of message types.  Set this however you like. */
   uint32 what;

   /** Default Constructor. */
   Message() : what(0) {/* empty */}

   /** Constructor.
    *  @param what The 'what' member variable will be set to the value you specify here.
    */
   explicit Message(uint32 what) : what(what) {/* empty */}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   Message(const Message & rhs) : FlatCountable(), Cloneable() {*this = rhs;}

   /** Destructor. */
   virtual ~Message() {/* empty */}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   Message & operator=(const Message & rhs);

   /** Comparison operator.  Two Message are considered equal iff their what codes are equal,
    *  and their sets of fields are equal.  Field ordering is not considered.  Note that this
    *  operation can be expensive for large Messages!
    * @param rhs the Message to compare this Message against
    */
   bool operator ==(const Message & rhs) const;

   /** @copydoc DoxyTemplate::operator!=(const DoxyTemplate &) const */
   bool operator !=(const Message & rhs) const {return !(*this == rhs);}

   /** Retrieve information about the given field in this message.
    *  @param fieldName Name of the field to look for.
    *  @param type If non-NULL, On sucess the type code of the found field is copied into this value.
    *  @param c If non-NULL, On success, the number of elements in the found field is copied into this value.
    *  @param fixed_size If non-NULL, On success, (*fixed_size) is set to reflect whether the returned field's objects are all the same size, or not.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the requested field couldn't be found.
    */
   status_t GetInfo(const String & fieldName, uint32 *type, uint32 *c = NULL, bool *fixed_size = NULL) const;

   /** Returns the number of fields of the given type that are present in the Message.
    *  @param type The type of field to count, or B_ANY_TYPE to count all field types.
    *  @return The number of matching fields
    */
   uint32 GetNumNames(uint32 type = B_ANY_TYPE) const;

   /** Returns true if there are any fields of the given type in the Message.
    *  @param type The type of field to check for, or B_ANY_TYPE to check for any field type.
    *  @return True iff any fields of the given type exist.
    */
   bool HasNames(uint32 type = B_ANY_TYPE) const {return (GetNumNames(type) > 0);}

   /** @return true iff there are no fields in this Message. */
   bool IsEmpty() const {return (_entries.IsEmpty());}

   /** Returns the type code of the field with the given name, or (defaultTypeCode) if the field doesn't exist.
     * @param fieldName the name of the field to query
     * @param defaultTypeCode the value to return if the field doesn't exist.  Default to B_ANY_TYPE.
     * @returns the typecode of the specified field, or (defaultTypeCode)
     */
   uint32 GetFieldTypeForName(const String & fieldName, uint32 defaultTypeCode = B_ANY_TYPE) const;

   /** Prints debug info describing the contents of this Message to stdout.
     * @param optFile If non-NULL, the text will be printed to this file.  If left as NULL, stdout will be used as a default.
     * @param maxRecurseLevel The maximum level of nested sub-Messages that we will print.  Defaults to MUSCLE_NO_LIMIT.
     * @param indentLevel Number of spaces to indent each printed line.  Used while recursing to format nested messages text nicely
     */
   void PrintToStream(FILE * optFile = NULL, uint32 maxRecurseLevel = MUSCLE_NO_LIMIT, int indentLevel = 0) const;

   /** Same as PrintToStream(), only the state of the Message is returned
    *  as a String instead of being printed to stdout.
    *  @param maxRecurseLevel The maximum level of nested sub-Messages that we will generate.  Defaults to MUSCLE_NO_LIMIT.
    *  @param indentLevel Number of spaces to indent each generate line.  Used while recursing to format nested messages text nicely
    *  @returns a String representation of this Message, for debugging
    */
   String ToString(uint32 maxRecurseLevel = MUSCLE_NO_LIMIT, int indentLevel = 0) const;

   /** Same as ToString(), except the text is added to the given String instead
    *  of being returned as a new String.
    *  @param s the String to add our generated text to
     * @param maxRecurseLevel The maximum level of nested sub-Messages that we will generate.  Defaults to MUSCLE_NO_LIMIT.
     * @param indentLevel Number of spaces to indent each generate line.  Used while recursing to format nested messages text nicely
    */
   void AddToString(String & s, uint32 maxRecurseLevel = MUSCLE_NO_LIMIT, int indentLevel = 0) const;

   /** Renames a field.
    *  @param old_entry Field name to rename from.
    *  @param new_entry Field name to rename to.  If a field with this name already exists,
    *                   it will be replaced.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if a field named (old_entry) couldn't be found.
    */
   status_t Rename(const String & old_entry, const String & new_entry);

   /** Returns false (Messages can be of various sizes, depending on how many fields they have, etc.) */
   virtual bool IsFixedSize() const {return false;}

   /** Returns B_MESSAGE_TYPE */
   virtual uint32 TypeCode() const {return B_MESSAGE_TYPE;}

   /** Returns the number of bytes it would take to flatten this Message into a byte buffer. */
   virtual uint32 FlattenedSize() const;

   /**
    *  Converts this Message into a flattened buffer of bytes that can be saved to disk
    *  or sent over a network, and later converted back into an identical Message object.
    *  @param flat the DataFlattener to use to write out the serialized bytes
    */
   virtual void Flatten(DataFlattener flat) const;

   /**
    *  Convert the given byte buffer back into a Message.  Any previous contents of
    *  this Message will be erased, and replaced with the data specified in the byte buffer.
    *  @param unflat the DataUnflattener to use to read the flattened data bytes.
    *  @return B_NO_ERROR if the buffer was successfully Unflattened, or an error code if there
    *          was an error (usually meaning the buffer was corrupt, or out-of-memory)
    */
   virtual status_t Unflatten(DataUnflattener & unflat);

   /** Adds a new string to the Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param val The string to add
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddString(const String & fieldName, const String & val);

   /** Adds a new int8 to the Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param val The int8 to add
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddInt8(const String & fieldName, int8 val);

   /** Adds a new int16 to the Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param val The int16 to add
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddInt16(const String & fieldName, int16 val);

   /** Adds a new int32 to the Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param val The int32 to add
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddInt32(const String & fieldName, int32 val);

   /** Adds a new int64 to the Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param val The int64 to add
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddInt64(const String & fieldName, int64 val);

   /** Adds a new boolean to the Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param val The boolean to add
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddBool(const String & fieldName, bool val);

   /** Adds a new float to the Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param val The float to add
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddFloat(const String & fieldName, float val);

   /** Adds a new double to the Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param val The double to add
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddDouble(const String & fieldName, double val);

   /** Adds a new Message to the Message.
    *  Note that this method is less efficient than the
    *  AddMessage(MessageRef) method, as this method
    *  necessitates copying all the data in (msg).
    *  @param fieldName Name of the field to add (or add to)
    *  @param msg The Message to add
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddMessage(const String & fieldName, const Message & msg) {return AddMessage(fieldName, GetMessageFromPool(msg));}

   /** Adds a new Message to the Message.
    *  Note that this method is more efficient than the
    *  AddMessage(const Message &) method, as this method
    *  need not make a copy of the referenced Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param msgRef Reference to the Message to add
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddMessage(const String & fieldName, const MessageRef & msgRef);

   /** Convenience method:  Calls SaveToArchive() on the specified object to save its
    *  state into a Message, then adds the resulting Message into this Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param obj The object to archive.  May be any type with a SaveToArchive() method.
    *  @return B_NO_ERROR on success, or an error code on failure.
    */
   template<class T> inline status_t AddArchiveMessage(const String & fieldName, const T & obj);

   /** Convenience method:  Same as AddArchiveMessage(), but this method only actually adds a
    *  Message if the (objRef) reference-argument is non-NULL.
    *  @param fieldName Name of the field to add (or add to)
    *  @param objRef Reference to the object to archive.  The object may be any type with a SaveToArchive() method.
    *  @return B_NO_ERROR on success (or if (objRef) was NULL), or an error code on failure.
    */
   template<typename T> inline status_t CAddArchiveMessage(const String & fieldName, const ConstRef<T> & objRef);

   /** Adds a new pointer value to the Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param ptr The pointer value to add
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddPointer(const String & fieldName, const void * ptr);

   /** Adds a new point to the Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param point The point to add
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddPoint(const String & fieldName, const Point & point);

   /** Adds a new rect to the Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param rect The rect to add
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddRect(const String & fieldName, const Rect & rect);

   /** Flattens a Flattenable object and adds the resulting bytes into this Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param obj The Flattenable object (or at least an object with TypeCode(), Flatten(),
    *             and FlattenedSize() methods).  Flatten() is called on this object, and the
    *             resulting bytes are appended to the given field in this Message.
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   template <class T> status_t AddFlat(const String & fieldName, const T & obj) {return AddFlatAux(fieldName, GetFlattenedByteBufferFromPool(obj), obj.TypeCode(), false);}

   /** Adds a reference to a FlatCountable object to the Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param ref The FlatCountable reference to add
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddFlat(const String & fieldName, const FlatCountableRef & ref);

   /** As above, but templated so that the caller doesn't need to upcast his reference-argument explicitly.
    *  @param fieldName Name of the field to add (or add to)
    *  @param ref The reference (to an object that subclasses FlatCountable) to add to the Message
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   template <class T> status_t AddFlat(const String & fieldName, const Ref<T> & ref) {return AddFlat(fieldName, FlatCountableRef(ref));}

   /** Adds a new ephemeral-tag-item to this Message.  Tags are references to arbitrary
    *  ref-countable C++ objects;  They can be added to a Message as a matter of convenience
    *  to the local program, but note that they are not persistent--they won't get
    *  Flatten()'d with the rest of the contents of the Message!
    *  @param fieldName Name of the field to add (or add to)
    *  @param tagRef Reference to the tag object to add.
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t AddTag(const String & fieldName, const RefCountableRef & tagRef);

   /** As above, but templated so that the caller doesn't need to do manual upcasting of the Ref in his code.
    *  @param fieldName Name of the field to add (or add to)
    *  @param tagRef Reference to (an object that subclasses RefCountable) to add.
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   template<class T> status_t AddTag(const String & fieldName, const Ref<T> & tagRef) {return AddTag(fieldName, RefCountableRef(tagRef));}

   /** Generic method, capable of adding any type of data to the Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param type The B_*_TYPE code indicating the type of data that will be added.
    *  @param data A pointer to the bytes of data to add.  The data is copied out of
    *              this byte buffer, and exactly how it is interpreted depends on (type).
    *              (data) may be NULL, in which case default objects (or uninitialized byte space) is added.
    *  Note:  If you are adding B_MESSAGE_TYPE, then (data) must point to a MessageRef
    *         object, and NOT a Message object, or flattened-Message-buffer!
    *         (This is inconsistent with BMessage::AddData, which expects a flattened BMessage buffer.  Sorry!)
    *  @param numBytes The number of bytes that are pointed to by (data).  If (type)
    *                  is a known type such as B_INT32_TYPE or B_RECT_TYPE, (numBytes)
    *                  may be a multiple of the datatype's size, allowing you to add
    *                  multiple entries with a single call.
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY if out of memory or B_BAD_ARGUMENT if
    *          (numBytes) isn't a multiple of (type)'s known size, or B_TYPE_MISMATCH if a type conflict occurred.
    */
   status_t AddData(const String & fieldName, uint32 type, const void *data, uint32 numBytes) {return AddDataAux(fieldName, data, numBytes, type, false);}

   /** Prepends a new string to the beginning of a field array in the Message.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param val The string to add or prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependString(const String & fieldName, const String & val);

   /** Prepends a new int8 to the beginning of a field array in the Message.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param val The int8 to add or prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependInt8(const String & fieldName, int8 val);

   /** Prepends a new int16 to the beginning of a field array in the Message.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param val The int16 to add or prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependInt16(const String & fieldName, int16 val);

   /** Prepends a new int32 to the beginning of a field array in the Message.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param val The int32 to add or prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependInt32(const String & fieldName, int32 val);

   /** Prepends a new int64 to the beginning of a field array in the Message.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param val The int64 to add or prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependInt64(const String & fieldName, int64 val);

   /** Prepends a new boolean to the beginning of a field array in the Message.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param val The boolean to add or prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependBool(const String & fieldName, bool val);

   /** Prepends a new float to the beginning of a field array in the Message.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param val The float to add or prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependFloat(const String & fieldName, float val);

   /** Prepends a new double to the beginning of a field array in the Message.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param val The double to add or prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependDouble(const String & fieldName, double val);

   /** Prepends a new Message to the beginning of a field array in the Message.
    *  Note that this method is less efficient than the AddMessage(MessageRef) method,
    *  as this method necessitates making a copy of (msg) and all the data it contains.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param msg The Message to add or prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependMessage(const String & fieldName, const Message & msg) {return PrependMessage(fieldName, GetMessageFromPool(msg));}

   /** Prepends a new Message to the beginning of a field array in the Message.
    *  Note that this method is more efficient than the
    *  PrependMessage(const Message &) method, as this method
    *  need not make a copy of the referenced Message.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param msgRef Reference to the Message to add or prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependMessage(const String & fieldName, const MessageRef & msgRef);

   /** Convenience method: Calls SaveToArchive() on the specified object to save its
    *  state into a Message, then prepends the resulting Message into this Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param obj The object to archive.  May be any type with a SaveToArchive() method.
    *  @return B_NO_ERROR on success, or B_OUT_OF_MEMORY if out of memory or SaveToArchive() failed.
    */
   template<class T> inline status_t PrependArchiveMessage(const String & fieldName, const T & obj);

   /** Convenience method:  Same as PrependArchiveMessage(), but this method only actually prepends a
    *  Message if the (objRef) reference-argument is non-NULL.
    *  @param fieldName Name of the field to add (or add to)
    *  @param objRef Reference to the object to archive.  The object may be any type with a SaveToArchive() method.
    *  @return B_NO_ERROR on success (or if (objRef) was NULL), or an error code on failure.
    */
   template<typename T> inline status_t CPrependArchiveMessage(const String & fieldName, const ConstRef<T> & objRef);

   /** Prepends a new pointer value to the beginning of a field array in the Message.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param ptr The pointer value to add or prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependPointer(const String & fieldName, const void * ptr);

   /** Prepends a new point to the beginning of a field array in the Message.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param point The point to add or prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependPoint(const String & fieldName, const Point & point);

   /** Prepends a new rectangle to the beginning of a field array in the Message.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param rect The rectangle to add or prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependRect(const String & fieldName, const Rect & rect);

   /** Flattens a Flattenable object and adds the resulting bytes into this Message.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param obj The Flattenable object (or at least an object with TypeCode(), Flatten(),
    *             and FlattenedSize() methods).  Flatten() is called on this object, and the
    *             resulting bytes are appended to the given field in this Message.
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   template <class T> status_t PrependFlat(const String & fieldName, const T & obj) {return AddFlatAux(fieldName, GetFlattenedByteBufferFromPool(obj), obj.TypeCode(), true);}

   /** Prepends a reference to a FlatCountable object to the Message.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param flatRef FlatCountable reference to prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependFlat(const String & fieldName, const FlatCountableRef & flatRef);

   /** As above, but templated so that the caller doesn't need to upcast his reference-argument explicitly.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param ref The reference (to an object that subclasses FlatCountable) to prepend
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   template <class T> status_t PrependFlat(const String & fieldName, const Ref<T> & ref) {return PrependFlat(fieldName, FlatCountableRef(ref));}

   /** Prepends a new ephemeral-tag-item to this Message.  Tags are references to arbitrary
    *  ref-countable C++ objects;  They can be added to a Message as a matter of convenience
    *  to the local program, but note that they are not persistent--they won't get
    *  Flatten()'d with the rest of the contents of the Message!
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param tagRef Reference to the tag object to add or prepend.
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   status_t PrependTag(const String & fieldName, const RefCountableRef & tagRef);

   /** As above, but templated so that the caller doesn't need to do manual upcasting of the Ref in his code.
    *  @param fieldName Name of the field to add (or prepend to)
    *  @param tagRef Reference to (an object that subclasses RefCountable) to add or prepend.
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY out of memory or B_TYPE_MISMATCH if a type conflict occurred
    */
   template<class T> status_t PrependTag(const String & fieldName, const Ref<T> & tagRef) {return PrependTag(fieldName, RefCountableRef(tagRef));}

   /** Generic method, capable of prepending any type of data to the Message.
    *  @param fieldName Name of the field to add (or add to)
    *  @param type The B_*_TYPE code indicating the type of data that will be added.
    *  @param data A pointer to the bytes of data to add.  The data is copied out of
    *              this byte buffer, and exactly how it is interpreted depends on (type).
    *              (data) may be NULL, in which case default objects (or uninitialized byte space) is added.
    *  Note:  If you are adding B_MESSAGE_TYPE, then (data) must point to a MessageRef
    *         object, and NOT a Message object, or flattened-Message-buffer!
    *         (This is inconsistent with BMessage::AddData, which expects a flattened BMessage buffer.  Sorry!)
    *  @param numBytes The number of bytes that are pointed to by (data).  If (type)
    *                  is a known type such as B_INT32_TYPE or B_RECT_TYPE, (numBytes)
    *                  may be a multiple of the datatype's size, allowing you to add
    *                  multiple entries with a single call.
    *  @return B_NO_ERROR on success, B_OUT_OF_MEMORY if out of memory or B_BAD_ARGUMENT if
    *          (numBytes) isn't a multiple of (type)'s known size, or B_TYPE_MISMATCH if a type conflict occurred.
    */
   status_t PrependData(const String & fieldName, uint32 type, const void *data, uint32 numBytes) {return AddDataAux(fieldName, data, numBytes, type, true);}

   /** Removes the (index)'th item from the given field entry.  If the field entry becomes
    *  empty, the field itself is removed also.
    *  @param fieldName Name of the field to remove an item from.
    *  @param index Index of the item to remove.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field name/index wasn't found.
    */
   status_t RemoveData(const String & fieldName, uint32 index = 0);

   /** Removes the given field name and its contents from the Message.
    *  @param fieldName Name of the field to remove.
    *  @return B_NO_ERROR on success, B_DATA_NOT_FOUND if the field name wasn't found.
    */
   status_t RemoveName(const String & fieldName) {return _entries.Remove(fieldName);}

   /** Clears all fields from the Message.
    *  @param releaseCachedBuffers If set true, any cached buffers we are holding will be immediately freed.
    *                              Otherwise, they will be kept around for future re-use.
    */
   void Clear(bool releaseCachedBuffers = false) {_entries.Clear(releaseCachedBuffers);}

   /** Returns a Message that is identical to this one, except that all data-values have been
     * set to their default/empty values.  The returned Message can be used as a template for
     * use in future calls to TemplatedFlatten(), TemplatedUnflatten(), TemplatedFlattenedSize(), etc.
     */
   MessageRef CreateMessageTemplate() const;

   /** Retrieve a string value from the Message.
    *  @param fieldName The field name to look for the string value under.
    *  @param index The index of the string item in its field entry.
    *  @param writeValueHere On success, this pointer will be pointing to a String containing the result.
    *  @return B_NO_ERROR if the string value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindString(const String & fieldName, uint32 index, const String ** writeValueHere) const;

   /** Retrieve a string value from the Message.
    *  @param fieldName The field name to look for the string value under.
    *  @param index The index of the string item in its field entry.
    *  @param writeValueHere On success, a pointer to the string value is written into this object.
    *  @return B_NO_ERROR if the string value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindString(const String & fieldName, uint32 index, const char * & writeValueHere) const;

   /** Retrieve a string value from the Message.
    *  @param fieldName The field name to look for the string value under.
    *  @param index The index of the string item in its field entry.
    *  @param writeValueHere On success, the value of the string is written into this object.
    *  @return B_NO_ERROR if the string value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindString(const String & fieldName, uint32 index, String & writeValueHere) const;

   /** Retrieve an int8 value from the Message.
    *  @param fieldName The field name to look for the int8 value under.
    *  @param index The index of the int8 item in its field entry.
    *  @param writeValueHere On success, the value of the int8 is written into this object.
    *  @return B_NO_ERROR if the int8 value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindInt8(const String & fieldName, uint32 index, int8 & writeValueHere) const {return FindDataItemAux(fieldName, index, B_INT8_TYPE, &writeValueHere, sizeof(int8));}

   /** Retrieve an int16 value from the Message.
    *  @param fieldName The field name to look for the int16 value under.
    *  @param index The index of the int16 item in its field entry.
    *  @param writeValueHere On success, the value of the int16 is written into this object.
    *  @return B_NO_ERROR if the int16 value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindInt16(const String & fieldName, uint32 index, int16 & writeValueHere) const {return FindDataItemAux(fieldName, index, B_INT16_TYPE, &writeValueHere, sizeof(int16));}

   /** Retrieve an int32 value from the Message.
    *  @param fieldName The field name to look for the int32 value under.
    *  @param index The index of the int32 item in its field entry.
    *  @param writeValueHere On success, the value of the int32 is written into this object.
    *  @return B_NO_ERROR if the int32 value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindInt32(const String & fieldName, uint32 index, int32 & writeValueHere) const {return FindDataItemAux(fieldName, index, B_INT32_TYPE, &writeValueHere, sizeof(int32));}

   /** Retrieve an int64 value from the Message.
    *  @param fieldName The field name to look for the int64 value under.
    *  @param index The index of the int64 item in its field entry.
    *  @param writeValueHere On success, the value of the int64 is written into this object.
    *  @return B_NO_ERROR if the int64 value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindInt64(const String & fieldName, uint32 index, int64 & writeValueHere) const {return FindDataItemAux(fieldName, index, B_INT64_TYPE, &writeValueHere, sizeof(int64));}

   /** Retrieve a boolean value from the Message.
    *  @param fieldName The field name to look for the boolean value under.
    *  @param index The index of the boolean item in its field entry.
    *  @param writeValueHere On success, the value of the boolean is written into this object.
    *  @return B_NO_ERROR if the boolean value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindBool(const String & fieldName, uint32 index, bool & writeValueHere) const {return FindDataItemAux(fieldName, index, B_BOOL_TYPE, &writeValueHere, sizeof(bool));}

   /** Retrieve a float value from the Message.
    *  @param fieldName The field name to look for the float value under.
    *  @param index The index of the float item in its field entry.
    *  @param writeValueHere On success, the value of the float is written into this object.
    *  @return B_NO_ERROR if the float value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindFloat(const String & fieldName, uint32 index, float & writeValueHere) const {return FindDataItemAux(fieldName, index, B_FLOAT_TYPE, &writeValueHere, sizeof(float));}

   /** Retrieve a double value from the Message.
    *  @param fieldName The field name to look for the double value under.
    *  @param index The index of the double item in its field entry.
    *  @param writeValueHere On success, the value of the double is written into this object.
    *  @return B_NO_ERROR if the double value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindDouble(const String & fieldName, uint32 index, double & writeValueHere) const {return FindDataItemAux(fieldName, index, B_DOUBLE_TYPE, &writeValueHere, sizeof(double));}

   /** Retrieve a Message value from the Message.
    *  Note that this method is less efficient than the FindMessage(MessageRef)
    *  method, because it forces the held Message's data to be copied into (msg)
    *  @param fieldName The field name to look for the Message value under.
    *  @param index The index of the Message item in its field entry.
    *  @param writeValueHere On success, the value of the Message is written into this object.
    *  @return B_NO_ERROR if the Message value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindMessage(const String & fieldName, uint32 index, Message & writeValueHere) const;

   /** Retrieve a MessageRef value from the Message.
    *  Note that this method is more efficient than the FindMessage(Message)
    *  method, because it the retrieved Message need not be copied.
    *  @param fieldName The field name to look for the Message value under.
    *  @param index The index of the Message item in its field entry.
    *  @param writeValueHere On success, the value of the Message is written into this object.
    *  @return B_NO_ERROR if the Message value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindMessage(const String & fieldName, uint32 index, MessageRef & writeValueHere) const;

   /** Same as above, except the returned reference is read-only.
    *  @param fieldName The field name to look for the Message value under.
    *  @param index The index of the Message item in its field entry.
    *  @param writeValueHere On success, the value of the Message is written into this object.
    *  @return B_NO_ERROR if the Message value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindMessage(const String & fieldName, uint32 index, ConstMessageRef & writeValueHere) const;

   /** Convenience method:  Retrieves a Message value from this Message,
    *  and if successful, calls SetFromArchive(msg) on the passed-in object.
    *  @param fieldName The field name to look for the Message value under.
    *  @param index The index of the Message item in its field entry.
    *  @param writeValueHere The object to call SetFromArchive(msg) on.  Since this method is templated,
    *                        this object may be of any type that has a SetFromArchive(const Message &) method.
    *  @return B_NO_ERROR if the Message value was found and SetFromArchive() succeeded, or B_DATA_NOT_FOUND otherwise.
    */
   template<class T> inline status_t FindArchiveMessage(const String & fieldName, uint32 index, T & writeValueHere) const
   {
      ConstMessageRef msg;
      return (FindMessage(fieldName, index, msg).IsOK()) ? writeValueHere.SetFromArchive(*msg()) : B_DATA_NOT_FOUND;
   }

   /** Convenience method:  Retrieves a Message value from this Message,
    *  and if successful, calls SetFromArchive(foundMsg) on the passed-in object.
    *  If the specified sub-Message is not found, this method calls SetFromArchive(defaultMsg) on the object instead.
    *  @param fieldName The field name to look for the Message value under.
    *  @param index The index of the Message item in its field entry.
    *  @param writeValueHere The object to call SetFromArchive(msg) on.  Since this method is templated,
    *                        this object may be of any type that has a SetFromArchive(const Message &) method.
    *  @param defaultMsg The Message to pass to SetFromArchive() if no sub-Message is found.  Defaults to GetEmptyMessage().
    *  @return The value returned by writeValueHere.SetFromArchive().
    */
   template<class T> inline status_t FindArchiveMessageWithDefault(const String & fieldName, uint32 index, T & writeValueHere, const Message & defaultMsg = GetEmptyMessage()) const
   {
      ConstMessageRef msg;
      return (FindMessage(fieldName, index, msg).IsOK()) ? writeValueHere.SetFromArchive(*msg()) : writeValueHere.SetFromArchive(defaultMsg);
   }

   /** Same as above, except that the index parameter is omitted (zero is implied).
    *  @param fieldName The field name to look for the Message value under.
    *  @param writeValueHere The object to call SetFromArchive(msg) on.  Since this method is templated,
    *                        this object may be of any type that has a SetFromArchive(const Message &) method.
    *  @param defaultMsg The Message to pass to SetFromArchive() if no sub-Message is found.  Defaults to GetEmptyMessage().
    *  @return The value returned by writeValueHere.SetFromArchive().
    */
   template<class T> inline status_t FindArchiveMessageWithDefault(const String & fieldName, T & writeValueHere, const Message & defaultMsg = GetEmptyMessage()) const {return FindArchiveMessageWithDefault(fieldName, 0, writeValueHere, defaultMsg);}

   /** Retrieve a pointer value from the Message.
    *  @param fieldName The field name to look for the pointer value under.
    *  @param index The index of the pointer item in its field entry.
    *  @param writeValueHere On success, the value of the pointer is written into this object.
    *  @return B_NO_ERROR if the pointer value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindPointer(const String & fieldName, uint32 index, void * & writeValueHere) const {return FindDataItemAux(fieldName, index, B_POINTER_TYPE, &writeValueHere, sizeof(void *));}

   /** Retrieve a point value from the Message.
    *  @param fieldName The field name to look for the point value under.
    *  @param index The index of the point item in its field entry.
    *  @param writeValueHere On success, the value of the point is written into this object.
    *  @return B_NO_ERROR if the point value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindPoint(const String & fieldName, uint32 index, Point & writeValueHere) const;

   /** Retrieve a rectangle value from the Message.
    *  @param fieldName The field name to look for the rectangle value under.
    *  @param index The index of the rectangle item in its field entry.
    *  @param writeValueHere On success, the value of the rectangle is written into this object.
    *  @return B_NO_ERROR if the rectangle value was found, or B_DATA_NOT_FOUND if it wasn't.
    */
   status_t FindRect(const String & fieldName, uint32 index, Rect & writeValueHere) const;

   /** Retrieve a flattened object from the Message.
    *  @param fieldName The field name to look for the flattened object value under.
    *  @param index The index of the flattened object item in its field entry.
    *  @param writeValueHere On success, the flattened object is copied into this object.
    *  @return B_NO_ERROR if the flattened object was found, or B_DATA_NOT_FOUND if it wasn't,
    *                     or B_TYPE_MISMATCH if the found item isn't compatible with (writeValueHere)
    */
   template <class T> status_t FindFlat(const String & fieldName, uint32 index, T & writeValueHere) const
   {
      uint32 arrayTypeCode;
      const muscle_private::MessageField * mf = GetMessageFieldAndTypeCode(fieldName, index, &arrayTypeCode);
      if (mf == NULL) return B_DATA_NOT_FOUND;

      if (writeValueHere.AllowsTypeCode(arrayTypeCode))
      {
         uint32 numBytes;
         const FlatCountable * fcPtr;
         const uint8 * ptr = FindFlatAux(mf, index, numBytes, &fcPtr);
         if (ptr)
         {
            DataUnflattener unflat(ptr, numBytes);
            return writeValueHere.Unflatten(unflat);
         }
         else if (fcPtr) return writeValueHere.CopyFrom(*fcPtr);
      }
      return B_TYPE_MISMATCH;
   }

   /** Retrieve a read-only FlatCountable reference from the Message.
    *  @param fieldName The field name to look for the FlatCountable reference under.
    *  @param index The index of the flattened object item in its field entry.
    *  @param writeValueHere On success, this reference will refer to the FlatCountable object.
    *  @return B_NO_ERROR if the flattened object was found, or B_DATA_NOT_FOUND if it wasn't,
    *                     or B_TYPE_MISMATCH if the found data item's type isn't compatible with (writeValueHere)
    */
   status_t FindFlat(const String & fieldName, uint32 index, ConstFlatCountableRef & writeValueHere) const;

   /** Retrieve a FlatCountable reference from the Message.
    *  @param fieldName The field name to look for the FlatCountable reference under.
    *  @param index The index of the flattened object item in its field entry.
    *  @param writeValueHere On success, this reference will refer to the FlatCountable object.
    *  @return B_NO_ERROR if the flattened object was found, or B_DATA_NOT_FOUND if it wasn't,
    *                     or B_TYPE_MISMATCH if the found data item's type isn't compatible with (writeValueHere)
    */
   status_t FindFlat(const String & fieldName, uint32 index, FlatCountableRef & writeValueHere) const;

   /** Templated convenience method; same as the above method but also performs any necessary downcasting of
    *  the found object (to match the passed-in Ref type) on your behalf.
    *  @param fieldName The field name to look for the FlatCountable reference under.
    *  @param index The index of the flattened object item in its field entry.
    *  @param writeValueHere On success, this reference will refer to the found object.
    *  @return B_NO_ERROR if the flattened object was found, or B_DATA_NOT_FOUND if it wasn't,
    *                     or B_TYPE_MISMATCH if the found data item's type isn't compatible with (writeValueHere)
    */
   template <class T> status_t FindFlat(const String & fieldName, uint32 index, ConstRef<T> & writeValueHere) const
   {
      ConstFlatCountableRef fcRef;
      MRETURN_ON_ERROR(FindFlat(fieldName, index, fcRef));
      return writeValueHere.SetFromRefCountableRef(fcRef);
   }

   /** Templated convenience method; same as the above method but also performs any necessary downcasting of
    *  the found object (to match the passed-in Ref type) on your behalf.
    *  @param fieldName The field name to look for the FlatCountable reference under.
    *  @param index The index of the flattened object item in its field entry.
    *  @param writeValueHere On success, this reference will refer to the found object.
    *  @return B_NO_ERROR if the flattened object was found, or B_DATA_NOT_FOUND if it wasn't,
    *                     or B_TYPE_MISMATCH if the found data item's type isn't compatible with (writeValueHere)
    */
   template <class T> status_t FindFlat(const String & fieldName, uint32 index, Ref<T> & writeValueHere) const
   {
      FlatCountableRef fcRef;
      MRETURN_ON_ERROR(FindFlat(fieldName, index, fcRef));
      return writeValueHere.SetFromRefCountableRef(fcRef);
   }

   /** Retrieves and returns an unflattened object of the specified type from the first data-item
     * in the specified flattened-Message field, or a default-constructed item of the specified type on failure.
     * @tparam T the type of item to return.
     * @param fieldName The field name to look for the flattened object under.
     * @returns The unflattened object that was found, if one was found and successfully unflattened, or a default-constructed item if it wasn't.
     */
   template <class T> T GetFlat(const String & fieldName) const
   {
      T ret;
      return (FindFlat(fieldName, ret).IsOK()) ? ret : GetDefaultObjectForType<T>();
   }

   /** Retrieves and returns an unflattened object of the specified type from the first data-item
     * in the specified flattened-Message field, or the specified fallback-object if it wasn't.
     * @param fieldName The field name to look for the flattened object under.
     * @param defaultValue the fallback-value to return on failure.
     * @param index The index of the object within (fieldName) to unflatten and return.  Defaults to zero (ie the first item)
     * @returns The unflattened object that was found, if one was found and successfully unflattened, or the specified fallback-item if it wasn't.
     */
   template <class T> T GetFlat(const String & fieldName, const T & defaultValue, uint32 index = 0) const
   {
      T ret;
      return (FindFlat(fieldName, index, ret).IsOK()) ? ret : defaultValue;
   }

   /** Convenience method:  Calls through to AddFlat(valueToAdd), but only if (valueToAdd) is not equal to a default-constructoed object of that type.
     * @param fieldName The field name of the field to (maybe) add the object to
     * @param valueToAdd The flattenable object to (maybe) add to the field
     * @returns B_NO_ERROR if (valueToAdd) was default-constructed (and therefore not added), or if it was added successfully, or an error code on error.
     */
   template<class T> status_t CAddFlat(const String & fieldName, const T & valueToAdd)
   {
      return (valueToAdd == GetDefaultObjectForType<T>()) ? B_NO_ERROR : AddFlat(fieldName, valueToAdd);
   }

   /** Convenience method:  Calls through to AddFlat(valueToAdd), but only if (valueToAdd) is not equal to (defaultValue).
     * @param fieldName The field name of the field to (maybe) add the object to
     * @param valueToAdd The flattenable object to (maybe) add to the field
     * @param defaultValue If (valueToAdd) is equal to (defaultValue), then this method will just return B_NO_ERROR without doing anything.
     * @returns B_NO_ERROR if (valueToAdd) was equal to (defaultValue) (and therefore not added), or if it was added successfully, or an error code on error.
     */
   template<class T> status_t CAddFlat(const String & fieldName, const T & valueToAdd, const T & defaultValue)
   {
      return (valueToAdd == defaultValue) ? B_NO_ERROR : AddFlat(fieldName, valueToAdd);
   }

   /** Convenience method:  Calls through to PrependFlat(valueToPrepend), but only if (valueToPrepend) is not equal to a default-constructoed object of that type.
     * @param fieldName The field name of the field to (maybe) prepend the object to
     * @param valueToPrepend The flattenable object to (maybe) prepend to the field
     * @returns B_NO_ERROR if (valueToPrepend) was default-constructed (and therefore not prepend), or if it was prepend successfully, or an error code on error.
     */
   template<class T> status_t CPrependFlat(const String & fieldName, const T & valueToPrepend)
   {
      return (valueToPrepend == GetDefaultObjectForType<T>()) ? B_NO_ERROR : PrependFlat(fieldName, valueToPrepend);
   }

   /** Convenience method:  Calls through to PrependFlat(valueToPrepend), but only if (valueToPrepend) is not equal to (defaultValue).
     * @param fieldName The field name of the field to (maybe) add the object to
     * @param valueToPrepend The flattenable object to (maybe) add to the field
     * @param defaultValue If (valueToPrepend) is equal to (defaultValue), then this method will just return B_NO_ERROR without doing anything.
     * @returns B_NO_ERROR if (valueToPrepend) was equal to (defaultValue) (and therefore not added), or if it was added successfully, or an error code on error.
     */
   template<class T> status_t CPrependFlat(const String & fieldName, const T & valueToPrepend, const T & defaultValue)
   {
      return (valueToPrepend == defaultValue) ? B_NO_ERROR : PrependFlat(fieldName, valueToPrepend);
   }

   /** Retrieve an ephemeral-tag-item from the Message.
    *  @param fieldName Name of the field to look for the tag under.
    *  @param index The index of the tag item in its field entry.
    *  @param writeValueHere On success, this object becomes a reference to the found tag object.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the tag couldn't be found.
    */
   status_t FindTag(const String & fieldName, uint32 index, RefCountableRef & writeValueHere) const;

   /** Same as above, but the returned reference is read-only.
    *  @param fieldName Name of the field to look for the tag under.
    *  @param index The index of the tag item in its field entry.
    *  @param writeValueHere On success, this object becomes a reference to the found tag object.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the tag couldn't be found.
    */
   status_t FindTag(const String & fieldName, uint32 index, ConstRefCountableRef & writeValueHere) const;

   /** Templated convenience method; same as the above method but also performs any necessary downcasting of
    *  the found object (to match the passed-in Ref type) on your behalf.
    *  @param fieldName The field name to look for the RefCountable reference under.
    *  @param index The index of the ref-countable object item in its field entry.
    *  @param writeValueHere On success, this reference will refer to the found object.
    *  @return B_NO_ERROR if the object was found, or B_DATA_NOT_FOUND if it wasn't,
    *                     or B_TYPE_MISMATCH if the found data item's type isn't compatible with (writeValueHere)
    */
   template <class T> status_t FindTag(const String & fieldName, uint32 index, ConstRef<T> & writeValueHere) const
   {
      ConstRefCountableRef rcRef;
      MRETURN_ON_ERROR(FindTag(fieldName, index, rcRef));
      return writeValueHere.SetFromRefCountableRef(rcRef);
   }

   /** As above, but the returned tag reference is mutable.
    *  @param fieldName The field name to look for the RefCountable reference under.
    *  @param index The index of the ref-countable object item in its field entry.
    *  @param writeValueHere On success, this reference will refer to the found object.
    *  @return B_NO_ERROR if the object was found, or B_DATA_NOT_FOUND if it wasn't,
    *                     or B_TYPE_MISMATCH if the found data item's type isn't compatible with (writeValueHere)
    */
   template <class T> status_t FindTag(const String & fieldName, uint32 index, Ref<T> & writeValueHere) const
   {
      RefCountableRef rcRef;
      MRETURN_ON_ERROR(FindTag(fieldName, index, rcRef));
      return writeValueHere.SetFromRefCountableRef(rcRef);
   }

   /** Retrieves and returns a RefCountableRef object of the specified type from the first data-item
     * in the specified RefCountableRef-field, or a default-constructed item of the specified type on failure.
     * @tparam T the type of item to return.
     * @param fieldName The field name to look for the RefCountableRef object under.
     * @returns The unflattened object that was found, if one was found and successfully unflattened, or a default-constructed item if it wasn't.
     */
   template <class T> T GetTag(const String & fieldName) const
   {
      T ret;
      return FindTag(fieldName, ret).IsOK() ? ret : GetDefaultObjectForType<T>();
   }

   /** Retrieves and returns an RefCountableRef object of the specified type from the first data-item
     * in the specified field, or the specified fallback-object if it wasn't.
     * @param fieldName The field name to look for the RefCountableRef object under.
     * @param defaultValue the fallback-value to return on failure.
     * @param index The index of the object within (fieldName) to return.  Defaults to zero (ie the first item)
     * @returns The object that was found, if one was found, or the specified fallback-item if it wasn't.
     */
   template <class T> T GetTag(const String & fieldName, const T & defaultValue, uint32 index = 0) const
   {
      T ret;
      return FindTag(fieldName, index, ret).IsOK() ? ret : defaultValue;
   }

   /** Retrieve a pointer to the raw data bytes of a stored message field of any type.
    *  @param fieldName The field name to retrieve the pointer to
    *  @param type The type code of the field you are interested, or B_ANY_TYPE if any type is acceptable.
    *  @param index Index of the data item to look for (within the field)
    *  @param data On success, a read-only pointer to the data bytes will be written into this object.
    *  @note If you are retrieving B_MESSAGE_TYPE, then (data) will be set to point to a MessageRef
    *        object, and NOT a Message object, or flattened-Message-buffer!
    *        (This is inconsistent with BMessage::FindData, which returns a flattened BMessage buffer.  Sorry!)
    *  @param numBytes On success, the number of bytes in the returned data array will be written into this object.  (May be NULL)
    *  @return B_NO_ERROR if the data pointer was retrieved successfully, or an error code if it wasn't.
    */
   status_t FindData(const String & fieldName, uint32 type, uint32 index, const void **data, uint32 *numBytes) const;

   /** As above, only (index) isn't specified.  It is assumed to be zero.
    *  @param fieldName The field name to retrieve the pointer to
    *  @param type The type code of the field you are interested, or B_ANY_TYPE if any type is acceptable.
    *  @param data On success, a read-only pointer to the data bytes will be written into this object.
    *  @note If you are retrieving B_MESSAGE_TYPE, then (data) will be set to point to a MessageRef
    *        object, and NOT a Message object, or flattened-Message-buffer!
    *        (This is inconsistent with BMessage::FindData, which returns a flattened BMessage buffer.  Sorry!)
    *  @param numBytes On success, the number of bytes in the returned data array will be written into this object.  (May be NULL)
    *  @return B_NO_ERROR if the data pointer was retrieved successfully, or an error code if it wasn't.
    */
   status_t FindData(const String & fieldName, uint32 type, const void **data, uint32 *numBytes) const {return FindData(fieldName, type, 0, data, numBytes);}

   /** As above, only this returns a pointer to an array that you can write directly into, for efficiency.
    *  You should be very careful with this method, though, as you will be writing into the guts of this Message.
    *  The returned pointer is not guaranteed to be valid after the Message is modified by any method!
    *  @see FindData(...)
    *  @param fieldName The field name to retrieve the pointer to
    *  @param type The type code of the field you are interested, or B_ANY_TYPE if any type is acceptable.
    *  @param index Index of the data item to look for (within the field)
    *  @param data On success, a read/write pointer to the data bytes will be written into this object.
    *  @note If you are retrieving B_MESSAGE_TYPE, then (data) will be set to point to a MessageRef
    *        object, and NOT a Message object, or flattened-Message-buffer!
    *        (This is inconsistent with BMessage::FindData, which returns a flattened BMessage buffer.  Sorry!)
    *  @param numBytes On success, the number of bytes in the returned data array will be written into this object.  (May be NULL)
    *  @return B_NO_ERROR if the data pointer was retrieved successfully, or an error code if it wasn't.
    */
   status_t FindDataPointer(const String & fieldName, uint32 type, uint32 index, void **data, uint32 *numBytes) const;

   /** As above, only (index) isn't specified.  It is assumed to be zero.
    *  @param fieldName The field name to retrieve the pointer to
    *  @param type The type code of the field you are interested, or B_ANY_TYPE if any type is acceptable.
    *  @param data On success, a read/write pointer to the data bytes will be written into this object.
    *  @note  If you are retrieving B_MESSAGE_TYPE, then (data) will be set to point to a MessageRef
    *         object, and NOT a Message object, or flattened-Message-buffer!
    *         (This is inconsistent with BMessage::FindData, which returns a flattened BMessage buffer.  Sorry!)
    *  @param numBytes On success, the number of bytes in the returned data array will be written into this object.  (May be NULL)
    *  @return B_NO_ERROR if the data pointer was retrieved successfully, or an error code if it wasn't.
    */
   status_t FindDataPointer(const String & fieldName, uint32 type, void **data, uint32 *numBytes) const {return FindDataPointer(fieldName, type, 0, data, numBytes);}

///@{
   /** Convenience method that calls through to its like-named counterpart with
     * a default value of zero for the index argument.  This allows you to handle the most common case
     * (storing a single value inside a field name) without having to constantly type FindXXX(fieldName, 0, myVar) for every call.
     * @param fieldName The field name to look for the value under.
     * @param writeValueHere On success, the found value is written into the variable indicated by this argument
     * @return B_NO_ERROR if the value value was found, or B_DATA_NOT_FOUND if it wasn't.
     */
   status_t FindString(const String & fieldName, const String ** writeValueHere) const {return FindString(fieldName, 0, writeValueHere);}
   status_t FindString(const String & fieldName, const char * & writeValueHere) const {return FindString(fieldName, 0, writeValueHere);}
   status_t FindInt8(const String & fieldName, int8 & writeValueHere) const {return FindInt8(fieldName, 0, writeValueHere);}
   status_t FindString(const String & fieldName, String & writeValueHere) const {return FindString(fieldName, 0, writeValueHere);}
   status_t FindInt16(const String & fieldName, int16 & writeValueHere) const {return FindInt16(fieldName, 0, writeValueHere);}
   status_t FindInt32(const String & fieldName, int32 & writeValueHere) const {return FindInt32(fieldName, 0, writeValueHere);}
   status_t FindInt64(const String & fieldName, int64 & writeValueHere) const {return FindInt64(fieldName, 0, writeValueHere);}
   status_t FindBool(const String & fieldName, bool & writeValueHere) const {return FindBool(fieldName, 0, writeValueHere);}
   status_t FindFloat(const String & fieldName, float & writeValueHere) const {return FindFloat(fieldName, 0, writeValueHere);}
   status_t FindDouble(const String & fieldName, double & writeValueHere) const {return FindDouble(fieldName, 0, writeValueHere);}
   status_t FindMessage(const String & fieldName, Message & writeValueHere) const {return FindMessage(fieldName, 0, writeValueHere);}
   status_t FindMessage(const String & fieldName, MessageRef & writeValueHere) const {return FindMessage(fieldName, 0, writeValueHere);}
   status_t FindMessage(const String & fieldName, ConstMessageRef & writeValueHere) const {return FindMessage(fieldName, 0, writeValueHere);}
   template<class T> inline status_t FindArchiveMessage(const String & fieldName, T & writeValueHere) const {return FindArchiveMessage(fieldName, 0, writeValueHere);}
   status_t FindPointer(const String & fieldName, void * & writeValueHere) const {return FindPointer(fieldName, 0, writeValueHere);}
   status_t FindPoint(const String & fieldName, Point & writeValueHere) const {return FindPoint(fieldName, 0, writeValueHere);}
   status_t FindRect(const String & fieldName, Rect & writeValueHere) const {return FindRect(fieldName, 0, writeValueHere);}

   template <class T> status_t FindFlat(const String & fieldName,           T & writeValueHere) const {return FindFlat(fieldName, 0, writeValueHere);}
   template <class T> status_t FindFlat(const String & fieldName, ConstRef<T> & writeValueHere) const {return FindFlat(fieldName, 0, writeValueHere);}
   template <class T> status_t FindFlat(const String & fieldName,      Ref<T> & writeValueHere) const {return FindFlat(fieldName, 0, writeValueHere);}
   status_t FindFlat(const String & fieldName,          ConstFlatCountableRef & writeValueHere) const {return FindFlat(fieldName, 0, writeValueHere);}
   status_t FindFlat(const String & fieldName,               FlatCountableRef & writeValueHere) const {return FindFlat(fieldName, 0, writeValueHere);}

   template <class T> status_t FindTag(const String & fieldName,            T & writeValueHere) const {return FindTag(fieldName, 0, writeValueHere);}
   template <class T> status_t FindTag(const String & fieldName,  ConstRef<T> & writeValueHere) const {return FindTag(fieldName, 0, writeValueHere);}
   template <class T> status_t FindTag(const String & fieldName,       Ref<T> & writeValueHere) const {return FindTag(fieldName, 0, writeValueHere);}
   status_t FindTag( const String & fieldName,           ConstRefCountableRef & writeValueHere) const {return FindTag(fieldName, 0, writeValueHere);}
   status_t FindTag( const String & fieldName,                RefCountableRef & writeValueHere) const {return FindTag(fieldName, 0, writeValueHere);}
///@}

   /** Replaces a string value in an existing Message field with a new value.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param newString The new string value to overwrite the old string with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceString(bool okayToAdd, const String & fieldName, uint32 index, const String & newString);

   /** Replaces an int8 value in an existing Message field with a new value.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param val The new int8 value to overwrite the old int8 with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceInt8(bool okayToAdd, const String & fieldName, uint32 index, int8 val);

   /** Replaces an int16 value in an existing Message field with a new value.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param val The new int16 value to overwrite the old int16 with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceInt16(bool okayToAdd, const String & fieldName, uint32 index, int16 val);

   /** Replaces an int32 value in an existing Message field with a new value.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param val The new int32 value to overwrite the old int32 with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceInt32(bool okayToAdd, const String & fieldName, uint32 index, int32 val);

   /** Replaces an int64 value in an existing Message field with a new value.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param val The new int64 value to overwrite the old int8 with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceInt64(bool okayToAdd, const String & fieldName, uint32 index, int64 val);

   /** Replaces a boolean value in an existing Message field with a new value.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param val The new boolean value to overwrite the old boolean with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceBool(bool okayToAdd, const String & fieldName, uint32 index, bool val);

   /** Replaces a float value in an existing Message field with a new value.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param val The new float value to overwrite the old float with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceFloat(bool okayToAdd, const String & fieldName, uint32 index, float val);

   /** Replaces a double value in an existing Message field with a new value.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param val The new double value to overwrite the old double with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceDouble(bool okayToAdd, const String & fieldName, uint32 index, double val);

   /** Replaces a pointer value in an existing Message field with a new value.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param ptr The new pointer value to overwrite the old pointer with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplacePointer(bool okayToAdd, const String & fieldName, uint32 index, const void * ptr);

   /** Replaces a point value in an existing Message field with a new value.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param point The new point value to overwrite the old point with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplacePoint(bool okayToAdd, const String & fieldName, uint32 index, const Point & point);

   /** Replaces a rectangle value in an existing Message field with a new value.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param rect The new rectangle value to overwrite the old rectangle with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceRect(bool okayToAdd, const String & fieldName, uint32 index, const Rect & rect);

   /** Replaces a Message value in an existing Message field with a new value.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param msg The new Message value to overwrite the old Message with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceMessage(bool okayToAdd, const String & fieldName, uint32 index, const Message & msg) {return ReplaceMessage(okayToAdd, fieldName, index, GetMessageFromPool(msg));}

   /** Replaces a Message value in an existing Message field with a new value.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param msgRef The new Message value to overwrite the old Message with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceMessage(bool okayToAdd, const String & fieldName, uint32 index, const MessageRef & msgRef);

   /** Convenience method: Replaces a Message value in an existing Message field with a new value
    *  that was generated by calling SaveToArchive() on the passed in object.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param obj The object to call SaveToArchive() on.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   template<class T> inline status_t ReplaceArchiveMessage(bool okayToAdd, const String & fieldName, uint32 index, const T & obj)
   {
      return ReplaceMessage(okayToAdd, fieldName, index, GetArchiveMessageFromPool(obj));
   }

   /** Flattens a Flattenable object and adds the resulting bytes into this Message.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName Name of the field to add (or add to)
    *  @param index The index of the entry within the field name to modify
    *  @param obj The Flattenable object (or at least an object with TypeCode(), Flatten(),
    *             and FlattenedSize() methods).  Flatten() is called on this object, and the
    *             resulting bytes are appended to the given field in this Message.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   template <class T> status_t ReplaceFlat(bool okayToAdd, const String & fieldName, uint32 index, const T & obj) {return ReplaceFlatAux(okayToAdd, fieldName, index, GetFlattenedByteBufferFromPool(obj), obj.TypeCode());}

   /** Replaces a FlatCountable reference in an existing Message field with a new reference.
    *  @param okayToAdd If set true, attempting to replace an reference that doesn't exist will cause the new reference to be added to the end of the field array, instead.  If false, attempting to replace a non-existent reference will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param newVal The new FlatCountableRef to overwrite the old reference with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceFlat(bool okayToAdd, const String & fieldName, uint32 index, const FlatCountableRef & newVal);

   /** As above, but templated so the caller doesn't need to do manual upcasting of the reference type.
    *  @param okayToAdd If set true, attempting to replace an reference that doesn't exist will cause the new reference to be added to the end of the field array, instead.  If false, attempting to replace a non-existent reference will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param newVal Reference to the object (which should be a subclass of FlatCountable) to overwrite the old reference with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   template<class T> status_t ReplaceFlat(bool okayToAdd, const String & fieldName, uint32 index, const Ref<T> & newVal) {return ReplaceFlat(okayToAdd, fieldName, index, FlatCountableRef(newVal));}

   /** Replaces a tag object in an existing Message field with a new tag object.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param newTag The new tag reference to overwrite the old tag reference with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceTag(bool okayToAdd, const String & fieldName, uint32 index, const RefCountableRef & newTag);

   /** As above, but templated for convenience so that the user doesn't need to do manual up-casting of his Ref type in his code.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name of an existing field to modify
    *  @param index The index of the entry within the field name to modify
    *  @param newTag The new tag reference to overwrite the old tag reference with.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   template<class T> status_t ReplaceTag(bool okayToAdd, const String & fieldName, uint32 index, const Ref<T> & newTag) {return ReplaceTag(okayToAdd, fieldName, index, newTag.GetRefCountableRef());}

   /** Replace one entry in a field of any type.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name to replace an entry in.
    *  @param type The uint32 of the field you are interested, or B_ANY_TYPE if any type is acceptable.
    *  @param index Index in the field to replace the data at.
    *  @param data The bytes representing the item you wish to replace an old item in the field with
    *  @param numBytes The number of bytes in your data array.
    *  Note:  If you are replacing B_MESSAGE_TYPE, then (data) must point to a MessageRef
    *         object, and NOT a Message object, or flattened-Message-buffer!
    *         (This is inconsistent with BMessage::ReplaceData, which expects a flattened BMessage buffer.  Sorry!)
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceData(bool okayToAdd, const String & fieldName, uint32 type, uint32 index, const void *data, uint32 numBytes);

   /** As above, only (index) isn't specified.  It is assumed to be zero.
    *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
    *  @param fieldName The field name to replace an entry in.
    *  @param type The uint32 of the field you are interested, or B_ANY_TYPE if any type is acceptable.
    *  @param data The bytes representing the item you wish to replace an old item in the field with
    *  @param numBytes The number of bytes in your data array.
    *  Note:  If you are replacing B_MESSAGE_TYPE, then (data) must point to a MessageRef
    *         object, and NOT a Message object, or flattened-Message-buffer!
    *         (This is inconsistent with BMessage::ReplaceData, which expects a flattened BMessage buffer.  Sorry!)
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
    */
   status_t ReplaceData(bool okayToAdd, const String & fieldName, uint32 type, const void *data, uint32 numBytes) {return ReplaceData(okayToAdd, fieldName, type, 0, data, numBytes);}

///@{
   /** Convenience method that calls through to its like-named counterpart with
     * a default value of zero for the index argument.  This allows you to handle the common case
     * (storing a single value inside a field name) without having to constantly type ReplaceXX(true, fieldName, 0, myVar) every time.
     *  @param okayToAdd If set true, attempting to replace an item that doesn't exist will cause the new item to be added to the end of the field array, instead.  If false, attempting to replace a non-existent item will cause B_DATA_NOT_FOUND to be returned with no side effects.
     *  @param fieldName the field name of an existing field to modify
     *  @param newVal the new value to overwrite the existing value with
     *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field wasn't found, or if (index) wasn't a valid index.
     */
   status_t ReplaceString(bool okayToAdd, const String & fieldName, const String & newVal) {return ReplaceString(okayToAdd, fieldName, 0, newVal);}
   status_t ReplaceInt8(bool okayToAdd, const String & fieldName, int8 newVal) {return ReplaceInt8(okayToAdd, fieldName, 0, newVal);}
   status_t ReplaceInt16(bool okayToAdd, const String & fieldName, int16 newVal) {return ReplaceInt16(okayToAdd, fieldName, 0, newVal);}
   status_t ReplaceInt32(bool okayToAdd, const String & fieldName, int32 newVal) {return ReplaceInt32(okayToAdd, fieldName, 0, newVal);}
   status_t ReplaceInt64(bool okayToAdd, const String & fieldName, int64 newVal) {return ReplaceInt64(okayToAdd, fieldName, 0, newVal);}
   status_t ReplaceBool(bool okayToAdd, const String & fieldName, bool newVal) {return ReplaceBool(okayToAdd, fieldName, 0, newVal);}
   status_t ReplaceFloat(bool okayToAdd, const String & fieldName, float newVal) {return ReplaceFloat(okayToAdd, fieldName, 0, newVal);}
   status_t ReplaceDouble(bool okayToAdd, const String & fieldName, double newVal) {return ReplaceDouble(okayToAdd, fieldName, 0, newVal);}
   status_t ReplacePointer(bool okayToAdd, const String & fieldName, const void * newVal) {return ReplacePointer(okayToAdd, fieldName, 0, newVal);}
   status_t ReplacePoint(bool okayToAdd, const String & fieldName, const Point & newVal) {return ReplacePoint(okayToAdd, fieldName, 0, newVal);}
   status_t ReplaceRect(bool okayToAdd, const String & fieldName, const Rect & newVal) {return ReplaceRect(okayToAdd, fieldName, 0, newVal);}
   status_t ReplaceMessage(bool okayToAdd, const String & fieldName, const Message & newVal) {return ReplaceMessage(okayToAdd, fieldName, 0, newVal);}
   status_t ReplaceMessage(bool okayToAdd, const String & fieldName, const MessageRef & newVal) {return ReplaceMessage(okayToAdd, fieldName, 0, newVal);}
   template <class T> status_t ReplaceArchiveMessage(bool okayToAdd, const String & fieldName, const T & newVal) {return ReplaceArchiveMessage(okayToAdd, fieldName, 0, newVal);}
   template <class T> status_t ReplaceFlat(bool okayToAdd, const String & fieldName, const T & newVal) {return ReplaceFlat(okayToAdd, fieldName, 0, newVal);}
   template <class T> status_t ReplaceFlat(bool okayToAdd, const String & fieldName, const Ref<T> & newVal) {return ReplaceFlat(okayToAdd, fieldName, 0, newVal);}
   status_t ReplaceFlat(bool okayToAdd, const String & fieldName, FlatCountableRef & newVal) {return ReplaceFlat(okayToAdd, fieldName, 0, newVal);}
   status_t ReplaceTag(bool okayToAdd, const String & fieldName, const RefCountableRef & newVal) {return ReplaceTag(okayToAdd, fieldName, 0, newVal);}
   template <class T> status_t ReplaceTag(bool okayToAdd, const String & fieldName, const Ref<T> & newVal) {return ReplaceTag(okayToAdd, fieldName, 0, newVal);}
///@}

   /** Convenience method:  Returns the first field name of the given type, or NULL if none is found.
     * @param optTypeCode If specified, only field names with the specified type will be considered.
     *                    If left as the default (B_ANY_TYPE), then the first field name of any type will be returned.
     * @returns a pointer to the returned field's name String on success, or NULL on failure.  Note that this pointer
     *          is not guaranteed to remain valid if the Message is modified.
     */
   const String * GetFirstFieldNameString(uint32 optTypeCode = B_ANY_TYPE) const {return GetExtremeFieldNameStringAux(optTypeCode, false);}

   /** Convenience method:  Returns the last field name of the given type, or NULL if none is found.
     * @param optTypeCode If specified, only field names with the specified type will be considered.
     *                    If left as the default (B_ANY_TYPE), then the last field name of any type will be returned.
     * @returns a pointer to the returned field's name String on success, or NULL on failure.  Note that this pointer
     *          is not guaranteed to remain valid if the Message is modified.
     */
   const String * GetLastFieldNameString(uint32 optTypeCode = B_ANY_TYPE) const {return GetExtremeFieldNameStringAux(optTypeCode, true);}

   /** Returns true iff there is a field with the given name and type present in the Message.
    *  @param fieldName the field name to look for.
    *  @param type the type to look for, or B_ANY_TYPE if type isn't important to you.
    *  @return true if such a field exists, else false.
    */
   bool HasName(const String & fieldName, uint32 type = B_ANY_TYPE) const {return (GetMessageField(fieldName, type) != NULL);}

   /** Returns the position of the specified field name within a MessageFieldNameIterator's name-traversal order.
     * @param fieldName the field name to inquire about
     * @returns returns 0 for the first field's name, 1 for the second, and so on).  Returns -1 if the field isn't found.
     * @note this method runs with O(N) complexity.
     */
   int32 IndexOfName(const String & fieldName) const {return _entries.IndexOfKey(fieldName);}

   /** Returns the number of values in the field with the given name and type in the Message.
    *  @param fieldName the field name to look for.
    *  @param type the type to look for, or B_ANY_TYPE if type isn't important to you.
    *  @return The number of values stored under (fieldName) if a field of the right type exists,
    *          or zero if the field doesn't exist or isn't of a matching type.
    */
   uint32 GetNumValuesInName(const String & fieldName, uint32 type = B_ANY_TYPE) const;

   /** Takes the field named (fieldName) in this message, and moves the field into (moveTo).
    *  Any data that was already in (moveTo) under (fieldName) will be replaced.
    *  @param fieldName Name of an existing field (in this Message) to be moved.
    *  @param moveTo A Message to move the specified field into.
    *  @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if the source field couldn't be found.
    */
   status_t MoveName(const String & fieldName, Message & moveTo) {return MoveName(fieldName, moveTo, fieldName);}

   /** Take the data under (oldFieldName) in this message, and moves it into (moveTo),
    *  where it will appear under the field name (newFieldName).
    *  Any data that was already in (moveTo) under (newFieldName) will be replaced.
    *  @param oldFieldName Name of an existing field (in this Message) to be moved.
    *  @param moveTo A Message to move the field into.
    *  @param newFieldName The name the field should have in the new Message.
    *  @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if the source field couldn't be found.
    */
   status_t MoveName(const String & oldFieldName, Message & moveTo, const String & newFieldName);

   /** Take the data under (fieldName) in this message, and copies it into (copyTo).
    *  Any data that was already in (copyTo) under (fieldName) will be replaced.
    *  @param fieldName Name of an existing field (in this Message) to be copied.
    *  @param copyTo A Message to copy the field into.
    *  @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if the source field couldn't be found.
    */
   status_t CopyName(const String & fieldName, Message & copyTo) const {return CopyName(fieldName, copyTo, fieldName);}

   /** Take the data under (oldFieldName) in this message, and copies it into (copyTo),
    *  where it will appear under the field name (newFieldName).
    *  Any data that was already in (copyTo) under (newFieldName) will be replaced.
    *  @param oldFieldName Name of an existing field (in this Message) to be copied.
    *  @param copyTo A Message to copy the field into.
    *  @param newFieldName The name the field should have in the new Message.
    *  @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if the source field couldn't be found.
    */
   status_t CopyName(const String & oldFieldName, Message & copyTo, const String & newFieldName) const;

   /** Swaps the contents of the field named (fieldName) in this Message with the contents of the like-named field in (swapWith).
    *  If a field with the given name is found only in one Message, it will be moved to the other Message.
    *  @param fieldName Name of an existing field (in either Message) to be swapped.
    *  @param swapWith A Message to swap a field's contents with
    *  @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if the field couldn't be found in either Message.
    */
   status_t SwapName(const String & fieldName, Message & swapWith);

   /** Take the data under (fieldName) in this message, and shares it into (shareTo).
    *  Any data that was under (fieldName) in (shareTo) will be replaced.
    *  This operation is similar to CopyName(), except that if there is more than
    *  one value present in the source field, no copy of the field data is made:
    *  instead, the field becomes shared between the two Messages, and changes
    *  to the field in one Message will be seen in the other.
    *  This method is more efficient than CopyName(), but you need to be careful
    *  when using it or you may get unexpected results...
    *  @param fieldName Name of an existing field to be shared.
    *  @param shareTo A Message to share the field into.
    *  @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if the source field couldn't be found.
    */
   status_t ShareName(const String & fieldName, Message & shareTo) const {return ShareName(fieldName, shareTo, fieldName);}

   /** Take the data under (oldFieldName) in this message, and shares it into (shareTo),
    *  where it will appear under the field name (newFieldName).
    *  Any data that was under (newFieldName) in (shareTo) will be replaced.
    *  This operation is similar to CopyName(), except that if there is more than
    *  one value present in the source field, no copy of the field data is made:
    *  instead, the field becomes shared between the two Messages, and changes
    *  to the field in one Message will be seen in the other.
    *  This method is more efficient than CopyName(), but you need to be careful
    *  when using it or you may get unexpected results...
    *  @param oldFieldName Name of an existing field to be shared.
    *  @param shareTo A Message to share the field into.
    *  @param newFieldName The name the field should have in the new Message.
    *  @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if the source field couldn't be found.
    */
   status_t ShareName(const String & oldFieldName, Message & shareTo, const String & newFieldName) const;

   /** Moves the field with the specified name to the beginning of the field-names-iteration-list.
     * @param fieldNameToMove Name of the field to move to the beginning of the iteration list.
     * @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if the source field couldn't be found.
     */
   status_t MoveNameToFront(const String & fieldNameToMove) {return _entries.MoveToFront(fieldNameToMove);}

   /** Moves the field with the specified name to the end of the field-names-iteration-list.
     * @param fieldNameToMove Name of the field to move to the end of the iteration list.
     * @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if the source field couldn't be found.
     */
   status_t MoveNameToBack(const String & fieldNameToMove) {return _entries.MoveToBack(fieldNameToMove);}

   /** Moves the field with the specified name to just before the second specified field name.
     * @param fieldNameToMove Name of the field to move
     * @param toBeforeMe Name of the field that (fieldNameToMove) should appear just before.
     * @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if the source field couldn't be found.
     */
   status_t MoveNameToBefore(const String & fieldNameToMove, const String & toBeforeMe) {return _entries.MoveToBefore(fieldNameToMove, toBeforeMe);}

   /** Moves the field with the specified name to just after the second specified field name.
     * @param fieldNameToMove Name of the field to move
     * @param toBehindMe Name of the field that (fieldNameToMove) should appear just after.
     * @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if the source field couldn't be found.
     */
   status_t MoveNameToBehind(const String & fieldNameToMove, const String & toBehindMe) {return _entries.MoveToBehind(fieldNameToMove, toBehindMe);}

   /** Moves the field with the specified name to the nth position in the field-names-iteration-list.
     * @param fieldNameToMove Name of the field to move
     * @param toPosition The position to move it to (0==first, 1=second, and so on)
     * @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if the source field couldn't be found.
     */
   status_t MoveNameToPosition(const String & fieldNameToMove, uint32 toPosition) {return _entries.MoveToPosition(fieldNameToMove, toPosition);}

   /** Examines the specified field to see if it is referenced more than once (eg by
     * another Message object).  If it is referenced more than once, makes a copy of the
     * field (including its contents) and replaces the shared field with the copy.
     * If the field is not referenced more than once, this method returns B_NO_ERROR
     * without doing anything.
     * This method is handy when you are about to modify a field in a Message and you want
     * to first make sure that the field isn't shared by any other Messages.
     * @param fieldName Name of the field to ensure the non-sharedness of.
     * @returns B_NO_ERROR on success (ie either the copy was made, or no copy was necessary)
     *          or B_DATA_NOT_FOUND if the specified field couldn't be found.
     */
   status_t EnsureFieldIsPrivate(const String & fieldName);

   /** Ensures that the data items held in (field) are stored as a contiguous array in memory,
     * and then returns a pointer to the beginning of the array.
     * Note:  In most cases, the contents of the field will already be stored contiguously, and so this
     * method will be very efficient (O(1)).  However, if the field was previously modified using any of
     * our Prepend*() methods, then the field data may need to be normalized into a contiguous
     * array before a pointer can be returned... in that case, this method will call Queue::Normalize()
     * to do that, which is an O(N) operation with respect to the number of data items stored in the field.
     * @param fieldName The name of the field we wish to have a pointer to the contents of.
     * @param retItemCount If non-NULL, on successful return this will hold the number of items pointed to by our return value.
     * @param type Type of field you are looking for, or B_ANY_TYPE if any type will do.  Defaults to B_ANY_TYPE.
     * @returns A pointer to the field's data on success, or NULL if a field with the specified name and type wasn't found.
     */
   void * GetPointerToNormalizedFieldData(const String & fieldName, uint32 * retItemCount, uint32 type = B_ANY_TYPE);

   /**
    * Swaps the contents and 'what' code of this Message with the specified Message.
    * This is a very efficient, O(1) operation.
    * @param swapWith Message whose contents should be swapped with this one.
    */
   void SwapContents(Message & swapWith) MUSCLE_NOEXCEPT;

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   Message(Message && rhs) MUSCLE_NOEXCEPT : what(0) {SwapContents(rhs);}

   /** @copydoc DoxyTemplate::operator=(DoxyTemplate &&) */
   Message & operator =(Message && rhs) MUSCLE_NOEXCEPT {SwapContents(rhs); return *this;}
#endif

   /** Sorts the iteration-order of this Message's field names into case-sensitive alphabetical order.
     * @param maxRecursions maximum depth to which this call should recurse to sub-Messages.  Defaults to zero (ie don't recurse)
     * @note to specify indefinite recursion, pass in MUSCLE_NO_LIMIT as an argument.
     */
   void SortFieldNames(uint32 maxRecursions=0)
   {
      _entries.SortByKey();
      if (maxRecursions > 0)
      {
         const uint32 subMaxRecursions = (maxRecursions == MUSCLE_NO_LIMIT) ? MUSCLE_NO_LIMIT : (maxRecursions-1);
         for (MessageFieldNameIterator fnIter = GetFieldNameIterator(B_MESSAGE_TYPE); fnIter.HasData(); fnIter++)
         {
            MessageRef subMsg;
            for (int32 i=0; FindMessage(fnIter.GetFieldName(), i, subMsg).IsOK(); i++) subMsg()->SortFieldNames(subMaxRecursions);
         }
      }
   }

   /** Sorts the data-items within a specified field using the default comparator for the field's type.
     * @param fieldName the field whose contents we should sort
     * @param from the first index to sort (defaults to zero, so that by default all items in the field will be sorted)
     * @param to one more than the the last index to sort (defaults to MUSCLE_NO_LIMIT, so that by default all items in the field will be sorted)
     */
   void SortDataInField(const String & fieldName, uint32 from=0, uint32 to=MUSCLE_NO_LIMIT);

   /** Returns true iff every one of our fields has a like-named, liked-typed, equal-length field in (rhs).
     * @param rhs The Message to check to see if it has a superset of our fields.
     * @param compareData If true, then the data in the fields will be compared also, and true will not be returned unless all the data items are equal.
     * @returns true If and only if our fields are a subset of (rhs)'s fields.
     */
   bool FieldsAreSubsetOf(const Message & rhs, bool compareData) const;

   /**
    * Iterates over the contents of this Message to compute a checksum.
    * Note that this method can be CPU-intensive, since it has to scan
    * everything in the Message.  Don't call it often if you want good
    * performance!
    * @param countNonFlattenableFields If true, non-flattenable fields (eg
    *                        those added with AddTag() will be included in the
    *                        checksum.  If false (the default), they will be
    *                        ignored.
    */
   uint32 CalculateChecksum(bool countNonFlattenableFields = false) const;

   /**
    * Returns an iterator that iterates over the names of the fields in this
    * message.  If (type) is B_ANY_TYPE, then all field names will be included
    * in the traversal.  Otherwise, only names of the given type will be included.
    * @param type Type of fields you wish to iterate over, or B_ANY_TYPE to iterate over all fields.
    * @param flags Bit-chord of HTIT_FLAG_* flags you want to use to affect the iteration behaviour.
    *              This bit-chord will get passed on to the underlying HashtableIterator.  Defaults
    *              to zero, which provides the default behaviour.
    */
   MessageFieldNameIterator GetFieldNameIterator(uint32 type = B_ANY_TYPE, uint32 flags = 0) const {return MessageFieldNameIterator(_entries.GetIterator(flags), type);}

   /**
    * As above, only starts the iteration at the given field name, instead of at the beginning
    * or end of the list of field names.
    * @param startFieldName the field name to start with.  If (startFieldName) isn't present, the iteration will be empty.
    * @param type Type of fields you wish to iterate over, or B_ANY_TYPE to iterate over all fields.
    * @param flags Bit-chord of HTIT_FLAG_* flags you want to use to affect the iteration behaviour.
    *              This bit-chord will get passed on to the underlying HashtableIterator.  Defaults
    *              to zero, which provides the default behaviour.
    */
   MessageFieldNameIterator GetFieldNameIteratorAt(const String & startFieldName, uint32 type = B_ANY_TYPE, uint32 flags = 0) const {return MessageFieldNameIterator(_entries.GetIteratorAt(startFieldName, flags), type);}

   /** Makes this Message into a "light-weight" copy of (rhs).
     * When this method returns, this object will look like a copy of (rhs), except that
     * it will share the data in (rhs)'s fields such that modifying the data in (rhs)'s
     * fields will modify the data in this Message, and vice versa.  Making a light-weight
     * copy can be significantly cheaper than doing a full copy (eg with the assignment operator),
     * but you need to be aware of the potential side effects if you then go on to modify
     * the contents of either Message's fields.  Use with caution!
     * @param rhs The Message to make this Message into a light-weight copy of.
     */
   void BecomeLightweightCopyOf(const Message & rhs) {what = rhs.what; _entries = rhs._entries;}

   /** Convenience method for retrieving a C-string pointer to a string data item inside a Message.
     * @param fn The field name to look for the string under.
     * @param defVal The pointer to return if no data item can be found.  Defaults to NULL.
     * @param idx The index to look under inside the field.  Defaults to zero.
     * @returns a Pointer to the String data item's Nul-terminated C string array on success, or (defVal) if the item could not be found.
     */
   inline const char * GetCstr(const String & fn, const char * defVal = NULL, uint32 idx = 0) const {const char * r; return (FindString( fn, idx, &r).IsOK()) ? r : defVal;}

   /** Convenience method for retrieving a pointer to a pointer data item inside a Message.
     * @param fn The field name to look for the pointer under.
     * @param defVal The pointer to return if no data item can be found.  Defaults to NULL.
     * @param idx The index to look under inside the field.  Defaults to zero.
     * @returns The requested pointer on success, or (defVal) on failure.
     */
   inline void * GetPointer(const String & fn, void * defVal = NULL, uint32 idx = 0) const {void * r; return (FindPointer(fn, idx,  r).IsOK()) ? r : defVal;}

   /** Convenience method for retrieving a pointer to a String data item inside a Message.
     * @param fn The field name to look for the String under.
     * @param defVal The pointer to return if no data item can be found.  Defaults to NULL.
     * @param idx The index to look under inside the field.  Defaults to zero.
     * @returns a Pointer to the String data item on success, or (defVal) if the item could not be found.
     */
   inline const String * GetStringPointer(const String & fn, const String * defVal=NULL, uint32 idx = 0) const {const String * r; return (FindString(fn, idx, &r).IsOK()) ? r : defVal;}

   /** Convenience method for retrieving a reference to a String data item inside a Message.
     * @param fn The field name to look for the String under.
     * @param idx The index to look under inside the field.  Defaults to zero.
     * @returns a read-only reference to the String, if found, or a read-only reference to an empty String otherwise.
     */
   inline const String & GetStringReference(const String & fn, uint32 idx = 0) const {return *GetStringPointer(fn, &GetEmptyString(), idx);}

#define DECLARE_MUSCLE_UNSIGNED_INTEGER_FIND_METHODS(bw)                                                                                               \
   inline status_t FindInt##bw (const String & fieldName, uint32 index, uint##bw & val) const {return FindInt##bw (fieldName, index, (int##bw &)val);} \
   inline status_t FindInt##bw (const String & fieldName,               uint##bw & val) const {return FindInt##bw (fieldName,        (int##bw &)val);}
   DECLARE_MUSCLE_UNSIGNED_INTEGER_FIND_METHODS(8);   ///< This macro defined Find methods with unsigned value arguments, so the user doesn't have to do ugly C-style casts to retrieve unsigned values.
   DECLARE_MUSCLE_UNSIGNED_INTEGER_FIND_METHODS(16);  ///< This macro defined Find methods with unsigned value arguments, so the user doesn't have to do ugly C-style casts to retrieve unsigned values.
   DECLARE_MUSCLE_UNSIGNED_INTEGER_FIND_METHODS(32);  ///< This macro defined Find methods with unsigned value arguments, so the user doesn't have to do ugly C-style casts to retrieve unsigned values.
   DECLARE_MUSCLE_UNSIGNED_INTEGER_FIND_METHODS(64);  ///< This macro defined Find methods with unsigned value arguments, so the user doesn't have to do ugly C-style casts to retrieve unsigned values.

#define DECLARE_MUSCLE_POINTER_FIND_METHODS(name, type)                                                                                \
   inline status_t Find##name (const String & fieldName, uint32 index, type * val) const {return Find##name (fieldName, index, *val);} \
   inline status_t Find##name (const String & fieldName,               type * val) const {return Find##name (fieldName,        *val);}
   DECLARE_MUSCLE_POINTER_FIND_METHODS(Bool,    bool);         ///< This macro defines old-style Find methods with pointer value arguments, for backwards compatibility.
   DECLARE_MUSCLE_POINTER_FIND_METHODS(Double,  double);       ///< This macro defines old-style Find methods with pointer value arguments, for backwards compatibility.
   DECLARE_MUSCLE_POINTER_FIND_METHODS(Float,   float);        ///< This macro defines old-style Find methods with pointer value arguments, for backwards compatibility.
   DECLARE_MUSCLE_POINTER_FIND_METHODS(Int8,    int8);         ///< This macro defines old-style Find methods with pointer value arguments, for backwards compatibility.
   DECLARE_MUSCLE_POINTER_FIND_METHODS(Int16,   int16);        ///< This macro defines old-style Find methods with pointer value arguments, for backwards compatibility.
   DECLARE_MUSCLE_POINTER_FIND_METHODS(Int32,   int32);        ///< This macro defines old-style Find methods with pointer value arguments, for backwards compatibility.
   DECLARE_MUSCLE_POINTER_FIND_METHODS(Int64,   int64);        ///< This macro defines old-style Find methods with pointer value arguments, for backwards compatibility.
   DECLARE_MUSCLE_POINTER_FIND_METHODS(Int8,    uint8);        ///< This macro defines old-style Find methods with pointer value arguments, for backwards compatibility.
   DECLARE_MUSCLE_POINTER_FIND_METHODS(Int16,   uint16);       ///< This macro defines old-style Find methods with pointer value arguments, for backwards compatibility.
   DECLARE_MUSCLE_POINTER_FIND_METHODS(Int32,   uint32);       ///< This macro defines old-style Find methods with pointer value arguments, for backwards compatibility.
   DECLARE_MUSCLE_POINTER_FIND_METHODS(Int64,   uint64);       ///< This macro defines old-style Find methods with pointer value arguments, for backwards compatibility.
   DECLARE_MUSCLE_POINTER_FIND_METHODS(Point,   Point);        ///< This macro defines old-style Find methods with pointer value arguments, for backwards compatibility.
   DECLARE_MUSCLE_POINTER_FIND_METHODS(Rect,    Rect);         ///< This macro defines old-style Find methods with pointer value arguments, for backwards compatibility.
   DECLARE_MUSCLE_POINTER_FIND_METHODS(String,  const char *); ///< This macro defines old-style Find methods with pointer value arguments, for backwards compatibility.

#define DECLARE_MUSCLE_CONVENIENCE_METHODS(name, type) \
   inline type Get##name(const String & fieldName, const type & defVal = type(), uint32 idx = 0) const {type r; return (Find##name (fieldName, idx, r).IsOK()) ? (const type &)r : defVal;} \
   inline status_t CAdd##name(    const String & fieldName, const type & value, const type & defVal = type())   {return (value == defVal) ? B_NO_ERROR : Add##name     (fieldName, value);}        \
   inline status_t CPrepend##name(const String & fieldName, const type & value, const type & defVal = type())   {return (value == defVal) ? B_NO_ERROR : Prepend##name (fieldName, value);}
   DECLARE_MUSCLE_CONVENIENCE_METHODS(Bool,    bool);            ///< This macro defines Get(), CAdd(), and CPrepend() methods for convience in common use cases.
   DECLARE_MUSCLE_CONVENIENCE_METHODS(Double,  double);          ///< This macro defines Get(), CAdd(), and CPrepend() methods for convience in common use cases.
   DECLARE_MUSCLE_CONVENIENCE_METHODS(Float,   float);           ///< This macro defines Get(), CAdd(), and CPrepend() methods for convience in common use cases.
   DECLARE_MUSCLE_CONVENIENCE_METHODS(Int8,    int8);            ///< This macro defines Get(), CAdd(), and CPrepend() methods for convience in common use cases.
   DECLARE_MUSCLE_CONVENIENCE_METHODS(Int16,   int16);           ///< This macro defines Get(), CAdd(), and CPrepend() methods for convience in common use cases.
   DECLARE_MUSCLE_CONVENIENCE_METHODS(Int32,   int32);           ///< This macro defines Get(), CAdd(), and CPrepend() methods for convience in common use cases.
   DECLARE_MUSCLE_CONVENIENCE_METHODS(Int64,   int64);           ///< This macro defines Get(), CAdd(), and CPrepend() methods for convience in common use cases.
   DECLARE_MUSCLE_CONVENIENCE_METHODS(Point,   Point);           ///< This macro defines Get(), CAdd(), and CPrepend() methods for convience in common use cases.
   DECLARE_MUSCLE_CONVENIENCE_METHODS(Rect,    Rect);            ///< This macro defines Get(), CAdd(), and CPrepend() methods for convience in common use cases.
   DECLARE_MUSCLE_CONVENIENCE_METHODS(String,  String);          ///< This macro defines Get(), CAdd(), and CPrepend() methods for convience in common use cases.
   DECLARE_MUSCLE_CONVENIENCE_METHODS(Message, MessageRef);      ///< This macro defines Get(), CAdd(), and CPrepend() methods for convience in common use cases.
   DECLARE_MUSCLE_CONVENIENCE_METHODS(Flat,    FlatCountableRef); ///< This macro defines Get(), CAdd(), and CPrepend() methods for convience in common use cases.

   DECLARE_MUSCLE_CONVENIENCE_METHODS(Tag,     RefCountableRef); ///< This macro defines Get(), CAdd(), and CPrepend() methods for convience in common use cases.
   DECLARE_STANDARD_CLONE_METHOD(Message);  ///< implements the standard Clone() method to copy a Message object.

   /**
    * This method calculates and returns a 64-bit checksum based only on the names, ordering,
    * types, and item-counts of its data-fields.  The fields' contents and the Message's "what" code are
    * not included in the checksum, but this call does recurse to child Messages.
    * @note this method is guaranteed never to return 0, so that 0 can
    *       be used as a guard-value in calling code if desired.
    */
   uint64 TemplateHashCode64() const;

   /**
     * Returns the number of bytes that TemplatedFlatten()  would need to flatten just the
     * payload-bytes of this Message into a byte-buffer, using the specified template-Message as
     * a guide.
     * @param templateMsg the Message to use as a guide for what data to read from this Message.
     * @see TemplatedFlatten() for details.
     */
   uint32 TemplatedFlattenedSize(const Message & templateMsg) const;

   /**
    * Writes the bare payload-data of this Message into (buffer), using (templateMsg)
    * as a guide for what field(s) to write.  Note that this method is optimized for
    * minimum data-size, and as such it does NOT write any meta-data at all; rather,
    * the passed-in template-Message represents the meta-data required to flatten or
    * unflatten the generated bytes.
    * @param templateMsg a template/example Message to use to determine what gets written
    *               into (buffer).  Only fields whose counterparts are present in
    *               (templateMsg) will be written to (buffer).  If a field is present
    *               in (templateMsg) but not in (this Message), then the values from
    *               (templateMsg) will be written to (buffer).
    * @param flat the DataFlattener to use to write out the bytes.  This object's
    *             max-bytes value should the value returned by TemplatedFlattenedSize(templateMsg)
    * @note The generated bytes can later be passed to TemplatedUnflatten() in order
    *       to recreate this Message (or at least the part of this Message) that matched
    *       the template, but you'll need to pass in the same (templateMsg) to
    *       TemplatedUnflatten() so that it can interpret the bytes correctly.
    */
   void TemplatedFlatten(const Message & templateMsg, DataFlattener flat) const;

   /** Reads the bare payload-data from (buf) into this Message, using the supplied
     * (templateMsg) as a guide for how to interpret the bytes in (buf).
     * @param templateMsg the template/example Message that was previously passed to
     *                    TemplatedFlatten() to generate the bytes in (buf).
     * @param unflat the DataUnflattener to use to read in the serialized-bar-payload-bytes.
     * @returns B_NO_ERROR on success, or some other value on failure (parse error or out of memory?)
     */
   status_t TemplatedUnflatten(const Message & templateMsg, DataUnflattener & unflat);

protected:
   /** Overridden to copy directly if (copyFrom) is a Message as well.
     * @param copyFrom the object to copy our new state from
     * @returns B_NO_ERROR on success or an error code on failure
     */
   virtual status_t CopyFromImplementation(const Flattenable & copyFrom);

private:
   // Helper functions
   status_t AddDataAux(const String & fieldName, uint32 type, const void *data, uint32 numBytes, bool prepend);

   // Given a known uint32, returns the size of an item of that type.
   // Returns zero if items of the given type are variable length.
   static uint32 GetElementSize(uint32 type);

   muscle_private::MessageField * GetMessageField(const String & fieldName, uint32 etc);
   const muscle_private::MessageField * GetMessageField(const String & fieldName, uint32 etc) const;
   muscle_private::MessageField * GetOrCreateMessageField(const String & fieldName, uint32 tc);
   const muscle_private::MessageField * GetMessageFieldAndTypeCode(const String & fieldName, uint32 index, uint32 * retTypeCode) const;

   status_t AddFlatAux(const String & fieldName, const FlatCountableRef & flat, uint32 etc, bool prepend);
   status_t AddDataAux(const String & fieldName, const void * data, uint32 size, uint32 etc, bool prepend);

   const uint8 * FindFlatAux(const muscle_private::MessageField * ada, uint32 index, uint32 & retNumBytes, const FlatCountable ** optRetFCPtr) const;
   status_t FindDataItemAux(const String & fieldName, uint32 index, uint32 tc, void * setValue, uint32 valueSize) const;

   status_t ReplaceFlatAux(bool okayToAdd, const String & fieldName, uint32 index, const FlatCountableRef & flat, uint32 tc);

   uint64 TemplateHashCode64Aux(uint32 & count) const;

   const String * GetExtremeFieldNameStringAux(uint32 optTypeCode, bool isLast) const
   {
      if (optTypeCode == B_ANY_TYPE) return isLast ? _entries.GetLastKey() : _entries.GetFirstKey();

      MessageFieldNameIterator iter(*this, optTypeCode, HTIT_FLAG_NOREGISTER|(isLast?HTIT_FLAG_BACKWARDS:0));
      return iter.HasData() ? &iter.GetFieldName() : NULL;
   }

   friend class muscle_private::MessageField;
   friend class MessageFieldNameIterator;
   Hashtable<String, muscle_private::MessageField> _entries;

   DECLARE_COUNTED_OBJECT(Message);
};

/** A macro to declare the necessary Template specializations so that the *Flat() methods do the right thing when called with a String/Point/Rect/Message object as their argument */
#define DECLARE_MUSCLE_FLAT_SPECIALIZERS(tp)                                                                                                                    \
template <> inline status_t Message::FindFlat<tp>(   const String & fieldName, uint32 index, tp & obj)  const  {return Find##tp(   fieldName, index, obj);}     \
template <> inline status_t Message::FindFlat<tp>(   const String & fieldName,               tp & obj)  const  {return Find##tp(   fieldName, obj);}            \
template <> inline status_t Message::AddFlat<tp>(    const String & fieldName,         const tp & obj)         {return Add##tp(    fieldName, obj);}            \
template <> inline status_t Message::PrependFlat<tp>(const String & fieldName,         const tp & obj)         {return Prepend##tp(fieldName, obj);}            \
template <> inline status_t Message::ReplaceFlat<tp>(bool okayToAdd, const String & fieldName, const tp & obj) {return Replace##tp(okayToAdd, fieldName, obj);} \
template <> inline status_t Message::ReplaceFlat<tp>(bool okayToAdd, const String & fieldName, uint32 index, const tp & obj)  {return Replace##tp( okayToAdd, fieldName, index, obj);}

DECLARE_MUSCLE_FLAT_SPECIALIZERS(String)
DECLARE_MUSCLE_FLAT_SPECIALIZERS(Point)
DECLARE_MUSCLE_FLAT_SPECIALIZERS(Rect)
DECLARE_MUSCLE_FLAT_SPECIALIZERS(Message)

/** Convenience method:  Gets a Message from the message pool, populates it using the flattened Message
 *  bytes held by (bb), and returns it.
 *  @param bb a ByteBuffer object to unflatten the Message from.
 *  @return Reference to a Message object, or a NULL ref on failure (out of memory or unflattening error)
 */
inline MessageRef GetMessageFromPool(const ByteBuffer & bb) {return GetMessageFromPool(bb.GetBuffer(), bb.GetNumBytes());}

/** Convenience method:  Gets a Message from the message pool, populates it using the flattened Message
 *  bytes held by (bb), and returns it.
 *  @param bbRef a ByteBufferRef referencing a ByteBuffer object to unflatten the Message from.
 *  @return Reference to a Message object, or a NULL ref on failure (out of memory or unflattening error)
 */
inline MessageRef GetMessageFromPool(const ConstByteBufferRef & bbRef) {return bbRef() ? GetMessageFromPool(*bbRef()) : MessageRef();}

/** Convenience method:  Gets a Message from the message pool, and populates it by calling
  * SaveToArchive(msg) on the passed in object.  Templates so that the passed-in object may be of any type.
  * @param pool The Message pool to get the Message object from.
  * @param objectToArchive The object to call SaveToArchive() on.
  * @returns a non-NULL MessageRef on success, or a NULL MessageRef on failure.
  */
template<class T> inline MessageRef GetArchiveMessageFromPool(ObjectPool<Message> & pool, const T & objectToArchive)
{
   MessageRef m = GetMessageFromPool(pool);
   if ((m())&&(objectToArchive.SaveToArchive(*m()).IsError())) m.Reset();
   return m;
}

/** Convenience method:  Gets a Message from the message pool, and populates it by calling
  * SaveToArchive(msg) on the passed in object.  Templates so that the passed-in object may be of any type.
  * @param objectToArchive The object to call SaveToArchive() on.
  * @returns a non-NULL MessageRef on success, or a NULL MessageRef on failure.
  */
template<class T> inline MessageRef GetArchiveMessageFromPool(const T & objectToArchive)
{
   MessageRef m = GetMessageFromPool();
   if ((m())&&(objectToArchive.SaveToArchive(*m()).IsError())) m.Reset();
   return m;
}

/** Convenience method:  Given a Message that was previously created via GetArchiveMessageFromPool<T>(),
  * creates and returns an object of type T, and calls SetFromArchive() on the object so that it
  * represents the state that was saved into the Message.
  * @tparam T the type of item to return a reference to.
  * @param msg The Message to extract the returned object's state-data from
  * @returns a reference to the new object on success, or a NULL reference on failure.
  */
template<class T> inline Ref<T> CreateObjectFromArchiveMessage(const Message & msg)
{
   Ref<T> newObjRef(newnothrow T);
   MRETURN_OOM_ON_NULL(newObjRef());
   MRETURN_ON_ERROR(newObjRef()->SetFromArchive(msg));
   return newObjRef;
}

/** As above, except this version takes a MessageRef instead of a Message.
  * @tparam T the type of item to return a reference to.
  * @param msgRef Reference to the Message to extract the returned object's state-data from.
  * @returns a reference to the new object on success, or a NULL reference on failure (or if msgRef was a NULL reference).
  */
template<class T> inline Ref<T> CreateObjectFromArchiveMessage(const ConstMessageRef & msgRef)
{
   return msgRef() ? CreateObjectFromArchiveMessage<T>(*msgRef()) : Ref<T>();
}

inline MessageFieldNameIterator :: MessageFieldNameIterator(const Message & msg, uint32 type, uint32 flags) : _typeCode(type), _iter(msg._entries.GetIterator(flags)) {if (_typeCode != B_ANY_TYPE) SkipNonMatchingFieldNames();}

// declared down here to make clang++ happy
template<class T> status_t Message :: AddArchiveMessage(const String & fieldName, const T & obj)
{
   return AddMessage(fieldName, GetArchiveMessageFromPool(obj));
}

// declared down here to make clang++ happy
template<typename T> status_t Message :: CAddArchiveMessage(const String & fieldName, const ConstRef<T> & objRef)
{
   return objRef() ? AddArchiveMessage(fieldName, *objRef()) : B_NO_ERROR;
}

// declared down here to make clang++ happy
template<class T> status_t Message :: PrependArchiveMessage(const String & fieldName, const T & obj)
{
   return PrependMessage(fieldName, GetArchiveMessageFromPool(obj));
}

// declared down here to make clang++ happy
template<typename T> status_t Message :: CPrependArchiveMessage(const String & fieldName, const ConstRef<T> & objRef)
{
   return objRef() ? PrependArchiveMessage(fieldName, *objRef()) : B_NO_ERROR;
}

/** Convenience method for printing out 'what'-codes
  * @param what a 32-bit what-code
  * @returns a human-readable String representation of that 'what'-code
  */
static inline String GetTypeCodeString(uint32 what)
{
   char buf[sizeof(uint32)+1];  // +1 for the NUL byte
   ::MakePrettyTypeCodeString(what, buf);
   return String(buf);
}

} // end namespace muscle

#endif /* _MUSCLEMESSAGE_H */

