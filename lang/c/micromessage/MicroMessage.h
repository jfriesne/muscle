#ifndef MicroMessage_h
#define MicroMessage_h

#include "support/MuscleSupport.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup micromessage The MicroMessage C function API 
 *  These functions are all defined in MicroMessage(.c,.h), and are stand-alone
 *  C functions that provide a way for C programs to use MUSCLE Messages.
 *  This is a very bare-bones implentation that does not do any dynamic memory
 *  allocation, and supports only linear addition of data to a Message.
 *  It is appropriate for severely constrained environments where
 *  even the \ref minimessage is too heavyweight.
 *  @{
 */

/** My own little boolean type, since C doesn't come with one built in. */
typedef char UBool;

/** Supported values for the UBool type */
enum {
   UFalse = 0,  /**< Constant value for boolean-false (zero) */
   UTrue        /**< Constant value for boolean-true (one)   */
};

/* This file contains a C API for a "super-minimalist" implementation of the MUSCLE  */
/* Message dictionary object.  This implementation sacrifices of flexibility         */
/* in exchange for a super-lightweight implementation that does no dynamic memory    */
/* allocation at all.                                                                */

/** Definition of our Point class -- two floats */
typedef struct _UPoint {
   float x;  /**< horizontal axis co-ordinate */
   float y;  /**< vertical axis co-ordinate   */
} UPoint;

/** Definition of our Rectangle class -- four floats */
typedef struct _URect {
   float left;   /**< left edge of the rectangle */
   float top;    /**< top edge of the rectangle */
   float right;  /**< right edge of the rectangle */
   float bottom; /**< bottom edge of the rectangle */
} URect;

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
/** Definition of our opaque handle to a UMessage object.  
  * Note that all fields in this struct are private and subject to change --
  * do not access them directly, call the functions declared below instead!
  */
typedef struct _UMessage {
   uint8 * _buffer;
   uint32 _bufferSize;
   uint32 _numValidBytes;
   uint8 * _currentAddField;
   UBool _isReadOnly;
   void * _parentMsg;     /* used during inline-child-UMessage construction, to notify parent that child's field is larger now */
   uint8 * _sizeField;    /* Pointer to our size-field in the parent UMessage, when we are an inline-child-UMessage being assembled. */
   uint8 * _readFieldCache;  /* a one-item LRU cache so we don't have to scan through all the fields all the time */
} UMessage;

/** This struct implements the state of an iterator for iterating over the field names in a UMessage */
typedef struct _UMessageFieldNameIterator {
   UMessage * _message;
   uint8 * _currentField;
   uint32 _typeCode;
} UMessageFieldNameIterator;
#endif

/** Initializes the state of the specified UMessageFieldNameIterator to point at the specified UMessage.
  * When this function returns, the iterator will be pointing to the first matching field in the UMessage (if there are any).
  * @param iter The iterator object to initialize.
  * @param msg The UMessage object the iterator is to examine.  (This object must remain valid while the iterator is in use)
  * @param typeCode Type-code of the fields the iteration should include, or B_ANY_TYPE if all types of field are of interest.
  */
void UMIteratorInitialize(UMessageFieldNameIterator * iter, const UMessage * msg, uint32 typeCode);

/** Returns the name of the message-field that the given iterator is currently pointing at, and
  * optionally some other information about the field as well.
  * @param iter The iterator object to query
  * @param optRetNumItemsInField If non-NULL, the number of values stored in the current field will be returned here.
  * @param optRetFieldType If non-NULL, the type-code of the current field will be returned here.
  * @returns A pointer to the C-string name of the current field, or NULL if the iterator isn't currently pointing at a valid field
  *          (e.g. because the Message has no fields in it, or because the iteration is complete)
  */
const char * UMIteratorGetCurrentFieldName(UMessageFieldNameIterator * iter, uint32 * optRetNumItemsInField, uint32 * optRetFieldType);

/** Advances the iterator to the next field in its Message, if there are any more.
  * @param iter the iterator to advance
  */
void UMIteratorAdvance(UMessageFieldNameIterator * iter);

/**
  * Initializes a UMessage object and associated it with the specified empty byte buffer.
  * This function will write the initial header data into (buf), and keep a pointer to
  * (buf) for use in future calls.
  * This function should be called on any UMessage object before using it to add new message data.
  * @param msg Pointer to the UMessage object to initialize. 
  * @param buf Pointer to the byte buffer the UMessage object should use for its 
  *            flattened-data storage.  This buffer must remain valid for as long
  *            as the UMessage object is in use.
  * @param numBytesInBuf Number of usable bytes at (buf).  Must be at least 12.
  * @param whatCode The 'what code' that this UMessage should hold.
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (e.g. buffer is too small)
  */
c_status_t UMInitializeToEmptyMessage(UMessage * msg, uint8 * buf, uint32 numBytesInBuf, uint32 whatCode);

/**
  * Initializes a UMessage object and associates it with the specified byte buffer that already
  * contains flattened message data.  The UMessage will be flagged as being read-only.
  * This function should be called on any UMessage object before using it to read received message data.
  * @param msg Pointer to the UMessage object to initialize. 
  * @param buf Pointer to the read-only byte buffer the UMessage object should examine
  *            to read existing flattened-data.  This buffer must remain valid for as long
  *            as the UMessage object is in use.
  * @param numBytesInBuf Number of usable bytes at (buf).  Must be at least 12.
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (e.g. buffer is too small, or contains invalid data)
  */
c_status_t UMInitializeWithExistingData(UMessage * msg, const uint8 * buf, uint32 numBytesInBuf);

/**
  * Initializes the UMessage to a well-defined but invalid state.  The UMessage will be read-only
  * and contain no data.
  * @param msg the UMessage object to initialize
  */
void UMInitializeToInvalid(UMessage * msg);

/**
  * @param msg The UMessage to query.
  * @returns UTrue iff (msg) is flagged as being read-only (i.e. we are reading a UMessage that was
  *          received from somewhere else) or UFalse if (msg) is read/write (i.e. we are constructing
  *          it from scratch)
  */
UBool UMIsMessageReadOnly(const UMessage * msg);


/**
  * @param msg The UMessage to query.
  * @returns UTrue iff (msg) is a valid UMessage, or UFalse if it is not.
  */
UBool UMIsMessageValid(const UMessage * msg);

/** 
  * Returns the number of data-fields in (msg).
  * @param msg The UMessage to query. 
  */
uint32 UMGetNumFields(const UMessage * msg);

/**
  * Returns the number of data-items within a data-field in (msg).
  * @param msg The UMessage to inquire into
  * @param fieldName the field-name to inquire about
  * @param typeCode If specified other than B_ANY_TYPE, than only items in a field that match this type will be counted.
  */
uint32 UMGetNumItemsInField(const UMessage * msg, const char * fieldName, uint32 typeCode);

/**
  * Returns the type-code of the specified field, or B_ANY_TYPE if the field is not present in the UMessage.
  * @param msg The UMessage to inquire into
  * @param fieldName the field-name to inquire about
  */
uint32 UMGetFieldTypeCode(const UMessage * msg, const char * fieldName);

/** 
  * Returns the current size of (msg)'s flattened-data-buffer, in bytes.
  * Note that this value includes only valid bytes, not "spare" bytes that are in the buffer but aren't currently being used.
  * @param msg The UMessage to query. 
  */
uint32 UMGetFlattenedSize(const UMessage * msg);

/** 
  * Returns the current the maximum number of bytes (msg)'s buffer can contain.
  * Note that this value includes all bytes in the buffer, whether they have had data written to them or not.
  * @param msg The UMessage to query. 
  */
uint32 UMGetMaximumSize(const UMessage * msg);

/** 
  * Returns a pointer to the buffer that (msg) is using.  This buffer contains UMGetFlattenedSize(msg) valid bytes of data.
  * @param msg The UMessage to query. 
  */
const uint8 * UMGetFlattenedBuffer(const UMessage * msg);

/** Prints the contents of this MMessage's buffer to stdout.  Useful for debugging.
  * @param msg The MicroMessage to print the contents of to stdout.
  * @param optFile If non-NULL, the file to print the output to.  If NULL, the output will go to stdout.
  */
void UMPrintToStream(const UMessage * msg, FILE * optFile);

/** Returns the 'what code' from the data buffer associated with (msg)
  * @param msg The UMessage associated with the data buffer.
  */
uint32 UMGetWhatCode(const UMessage * msg);

/** Sets the 'what code' in the data buffer associated with (msg)
  * @param msg The UMessage associated with the data buffer.
  * @param whatCode the new what-code to set in the data buffer.
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (e.g. msg's buffer is too small)
  */
c_status_t UMSetWhatCode(UMessage * msg, uint32 whatCode);

/** Adds an array of one or more boolean values to the UMessage.
  * @param msg The UMessage to add data to.
  * @param fieldName The field name to add the data to.
  * @param vals Pointer to an array of boolean values to be copied into (msg).
  * @param numVals The number of boolean values pointed to by (vals).
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (out of space, or field-name semantics would be violated?)
  */
c_status_t UMAddBools(UMessage * msg, const char * fieldName, const UBool * vals, uint32 numVals);

/** Convenience wrapper method for adding a single boolean value to the UMessage.  See UMAddBools() for details. */
static inline c_status_t UMAddBool(UMessage * msg, const char * fieldName, UBool val) {return UMAddBools(msg, fieldName, &val, 1);}

/** Adds an array of one or more int8 values to the UMessage.
  * @param msg The UMessage to add data to.
  * @param fieldName The field name to add the data to.
  * @param vals Pointer to an array of int8 values to be copied into (msg).
  * @param numVals The number of int8 values pointed to by (vals).
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (out of space, or field-name semantics would be violated?)
  */
c_status_t UMAddInt8s(UMessage * msg, const char * fieldName, const int8 * vals, uint32 numVals);

/** Convenience wrapper method for adding a single int8 value to the UMessage.  See UMAddInt8s() for details. */
static inline c_status_t UMAddInt8(UMessage * msg, const char * fieldName, int8 val) {return UMAddInt8s(msg, fieldName, &val, 1);}

/** Adds an array of one or more int16 values to the UMessage.
  * @param msg The UMessage to add data to.
  * @param fieldName The field name to add the data to.
  * @param vals Pointer to an array of int16 values to be copied into (msg).
  * @param numVals The number of int16 values pointed to by (vals).
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (out of space, or field-name semantics would be violated?)
  */
c_status_t UMAddInt16s(UMessage * msg, const char * fieldName, const int16 * vals, uint32 numVals);

/** Convenience wrapper method for adding a single int16 value to the UMessage.  See UMAddInt16s() for details. */
static inline c_status_t UMAddInt16(UMessage * msg, const char * fieldName, int16 val) {return UMAddInt16s(msg, fieldName, &val, 1);}

/** Adds an array of one or more int32 values to the UMessage.
  * @param msg The UMessage to add data to.
  * @param fieldName The field name to add the data to.
  * @param vals Pointer to an array of int32 values to be copied into (msg).
  * @param numVals The number of int32 values pointed to by (vals).
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (out of space, or field-name semantics would be violated?)
  */
c_status_t UMAddInt32s(UMessage * msg, const char * fieldName, const int32 * vals, uint32 numVals);

/** Convenience wrapper method for adding a single int32 value to the UMessage.  See UMAddInt32s() for details. */
static inline c_status_t UMAddInt32(UMessage * msg, const char * fieldName, int32 val) {return UMAddInt32s(msg, fieldName, &val, 1);}

/** Adds an array of one or more int64 values to the UMessage.
  * @param msg The UMessage to add data to.
  * @param fieldName The field name to add the data to.
  * @param vals Pointer to an array of int64 values to be copied into (msg).
  * @param numVals The number of int64 values pointed to by (vals).
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (out of space, or field-name semantics would be violated?)
  */
c_status_t UMAddInt64s(UMessage * msg, const char * fieldName, const int64 * vals, uint32 numVals);

/** Convenience wrapper method for adding a single int64 value to the UMessage.  See UMAddInt64s() for details. */
static inline c_status_t UMAddInt64(UMessage * msg, const char * fieldName, int64 val) {return UMAddInt64s(msg, fieldName, &val, 1);}

/** Adds an array of one or more floating point values to the UMessage.
  * @param msg The UMessage to add data to.
  * @param fieldName The field name to add the data to.
  * @param vals Pointer to an array of floating point values to be copied into (msg).
  * @param numVals The number of floating point values pointed to by (vals).
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (out of space, or field-name semantics would be violated?)
  */
c_status_t UMAddFloats(UMessage * msg, const char * fieldName, const float * vals, uint32 numVals);

/** Convenience wrapper method for adding a single floating point value to the UMessage.  See UMAddFloats() for details. */
static inline c_status_t UMAddFloat(UMessage * msg, const char * fieldName, float val) {return UMAddFloats(msg, fieldName, &val, 1);}

/** Adds an array of one or more double-precision floating point values to the UMessage.
  * @param msg The UMessage to add data to.
  * @param fieldName The field name to add the data to.
  * @param vals Pointer to an array of double-precision floating point values to be copied into (msg).
  * @param numVals The number of double-precision floating point values pointed to by (vals).
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (out of space, or field-name semantics would be violated?)
  */
c_status_t UMAddDoubles(UMessage * msg, const char * fieldName, const double * vals, uint32 numVals);

/** Convenience wrapper method for adding a single double-precision floating point value to the UMessage.  See UMAddDoubles() for details. */
static inline c_status_t UMAddDouble(UMessage * msg, const char * fieldName, double val) {return UMAddDoubles(msg, fieldName, &val, 1);}

/** Adds an array of one or more child sub-UMessages to the UMessage.
  * @param msg The UMessage to add data to.
  * @param fieldName The field name to add the data to.
  * @param messageArray Pointer to an array of UMessage values to be copied into (msg).
  * @param numVals The number of child sub-UMessages pointed to by (vals).
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (out of space, or field-name semantics would be violated?)
  * @note This method of adding child-UMessages requires copying all of the child-UMessages' data over from their
  *       buffers into the (msg)'s buffer.  As such, it may be inefficient, particularly if the child-UMessages are 
  *       large.  For a more efficient approach to message-composition, see the UMInlineAddMessage() function.
  */
c_status_t UMAddMessages(UMessage * msg, const char * fieldName, const UMessage * messageArray, uint32 numVals);

/** Convenience wrapper method for adding a single UMessage value to the UMessage.  See UMAddUMessages() for details. */
static inline c_status_t UMAddMessage(UMessage * msg, const char * fieldName, UMessage message) {return UMAddMessages(msg, fieldName, &message, 1);}

/** Adds an array of one or more UPoint values to the UMessage.
  * @param msg The UMessage to add data to.
  * @param fieldName The field name to add the data to.
  * @param pointArray Pointer to an array of UPoint values to be copied into (msg).
  * @param numVals The number of UPoint values pointed to by (vals).
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (out of space, or field-name semantics would be violated?)
  */
c_status_t UMAddPoints(UMessage * msg, const char * fieldName, const UPoint * pointArray, uint32 numVals);

/** Convenience wrapper method for adding a single UPoint value to the UMessage.  See UMAddUPoint() for details. */
static inline c_status_t UMAddPoint(UMessage * msg, const char * fieldName, UPoint point) {return UMAddPoints(msg, fieldName, &point, 1);}

/** Adds an array of one or more URect values to the UMessage.
  * @param msg The UMessage to add data to.
  * @param fieldName The field name to add the data to.
  * @param rectArray Pointer to an array of URect values to be copied into (msg).
  * @param numVals The number of URect values pointed to by (vals).
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (out of space, or field-name semantics would be violated?)
  */
c_status_t UMAddRects(UMessage * msg, const char * fieldName, const URect * rectArray, uint32 numVals);

/** Convenience wrapper method for adding a single URect value to the UMessage.  See UMAddRects() for details. */
static inline c_status_t UMAddRect(UMessage * msg, const char * fieldName, URect rect) {return UMAddRects(msg, fieldName, &rect, 1);}

/** Adds an array of one or more string values to the UMessage.
  * @param msg The UMessage to add data to.
  * @param fieldName The field name to add the data to.
  * @param stringArray Pointer to an array of string values to be copied into (msg).
  * @param numStrings The number of string values pointed to by (vals).
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (out of space, or field-name semantics would be violated?)
  */
c_status_t UMAddStrings(UMessage * msg, const char * fieldName, const char ** stringArray, uint32 numStrings);

/** Convenience wrapper method for adding a single string value to the UMessage.  See UMAddStrings() for details. */
static inline c_status_t UMAddString(UMessage * msg, const char * fieldName, const char * string) {return UMAddStrings(msg, fieldName, &string, 1);}

/** Adds a a "blob" of raw binary data to the UMessage.
  * @param msg The UMessage to add data to.
  * @param fieldName The field name to add the raw data to.
  * @param dataType The type-code to associate with the raw data.  (B_RAW_TYPE is often used here, or perhaps some other user-defined type code)
  * @param dataBytes Pointer to the bytes of raw data to add
  * @param numBytes Number of bytes pointed to by (dataBytes)
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (out of space, or field-name semantics would be violated?)
  */
c_status_t UMAddData(UMessage * msg, const char * fieldName, uint32 dataType, const void * dataBytes, uint32 numBytes);

/** This function can be used to create a sub-Message directly within its parent UMessage.
  * This can be more efficient than the usual UMAddMessage()/UMAddMessages() route, as it avoids
  * having to make a copy of the child UMessage after the child UMessage is complete.  
  * @param parentMsg The UMessage that the new child UMessage will be a child of.
  * @param fieldName The field name that the child UMessage should appear udner, inside the parent.
  * @param whatCode The what-code to assign to the child UMessage.
  * @returns a UMessage object whose data-buffer resides inside the data-buffer of the parent UMessage.
  *          Be sure to make any additions to the child UMessage before making any further additions to the
  *          parent UMessage, and once you have made a related addition to the parent UMessage, do not make
  *          any further additions to the child UMessage.  Breaking these rules will result in data corruption.
  */
UMessage UMInlineAddMessage(UMessage * parentMsg, const char * fieldName, uint32 whatCode);

/** Queries the UMessage for a particular boolean value.
  * @param msg The UMessage to query.
  * @param fieldName The field to name to look inside
  * @param idx The index of the boolean item to look for (e.g. 0 is the first in the array, 1 is the second, and so on)
  * @param retBool on success, the requested value is written into this location
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (field name not found, was of the wrong type, or the array was shorter than (idx+1) items)
  */
c_status_t UMFindBool(const UMessage * msg, const char * fieldName, uint32 idx, UBool * retBool);

/** Returns a pointer to the requested boolean value, or a default value (UFalse) on failure.
  * @param msg The UMessage to query.
  * @param fieldName The field name to look inside.
  * @param idx The index of the boolean to look for (e.g. 0 is the first in the field's array, 1 is the second, and so on)
  */
static inline UBool UMGetBool(const UMessage * msg, const char * fieldName, uint32 idx) {UBool r; return (UMFindBool(msg, fieldName, idx, &r)==CB_NO_ERROR)?r:((UBool)UFalse);}

/** Queries the UMessage for a particular int8 value.
  * @param msg The UMessage to query.
  * @param fieldName The field to name to look inside
  * @param idx The index of the int8 item to look for (e.g. 0 is the first in the array, 1 is the second, and so on)
  * @param retInt8 on success, the requested value is written into this location
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (field name not found, was of the wrong type, or the array was shorter than (idx+1) items)
  */
c_status_t UMFindInt8(const UMessage * msg, const char * fieldName, uint32 idx, int8 * retInt8);

/** Returns a pointer to the requested int8 value, or a default value (0) on failure.
  * @param msg The UMessage to query.
  * @param fieldName The field name to look inside.
  * @param idx The index of the int8 to look for (e.g. 0 is the first in the field's array, 1 is the second, and so on)
  */
static inline int8 UMGetInt8(const UMessage * msg, const char * fieldName, uint32 idx) {int8 r; return (UMFindInt8(msg, fieldName, idx, &r)==CB_NO_ERROR)?r:0;}

/** Queries the UMessage for a particular int16 value.
  * @param msg The UMessage to query.
  * @param fieldName The field to name to look inside
  * @param idx The index of the int16 item to look for (e.g. 0 is the first in the array, 1 is the second, and so on)
  * @param retInt16 on success, the requested value is written into this location
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (field name not found, was of the wrong type, or the array was shorter than (idx+1) items)
  */
c_status_t UMFindInt16(const UMessage * msg, const char * fieldName, uint32 idx, int16 * retInt16);

/** Returns a pointer to the requested int16 value, or a default value (0) on failure.
  * @param msg The UMessage to query.
  * @param fieldName The field name to look inside.
  * @param idx The index of the int16 to look for (e.g. 0 is the first in the field's array, 1 is the second, and so on)
  */
static inline int16 UMGetInt16(const UMessage * msg, const char * fieldName, uint32 idx) {int16 r; return (UMFindInt16(msg, fieldName, idx, &r)==CB_NO_ERROR)?r:0;}

/** Queries the UMessage for a particular int32 value.
  * @param msg The UMessage to query.
  * @param fieldName The field to name to look inside
  * @param idx The index of the int32 item to look for (e.g. 0 is the first in the array, 1 is the second, and so on)
  * @param retInt32 on success, the requested value is written into this location
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (field name not found, was of the wrong type, or the array was shorter than (idx+1) items)
  */
c_status_t UMFindInt32(const UMessage * msg, const char * fieldName, uint32 idx, int32 * retInt32);

/** Returns a pointer to the requested int32 value, or a default value (0) on failure.
  * @param msg The UMessage to query.
  * @param fieldName The field name to look inside.
  * @param idx The index of the int32 to look for (e.g. 0 is the first in the field's array, 1 is the second, and so on)
  */
static inline int32 UMGetInt32(const UMessage * msg, const char * fieldName, uint32 idx) {int32 r; return (UMFindInt32(msg, fieldName, idx, &r)==CB_NO_ERROR)?r:0;}

/** Queries the UMessage for a particular int64 value.
  * @param msg The UMessage to query.
  * @param fieldName The field to name to look inside
  * @param idx The index of the int64 item to look for (e.g. 0 is the first in the array, 1 is the second, and so on)
  * @param retInt64 on success, the requested value is written into this location
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (field name not found, was of the wrong type, or the array was shorter than (idx+1) items)
  */
c_status_t UMFindInt64(const UMessage * msg, const char * fieldName, uint32 idx, int64 * retInt64);

/** Returns a pointer to the requested int64 value, or a default value (0) on failure.
  * @param msg The UMessage to query.
  * @param fieldName The field name to look inside.
  * @param idx The index of the int64 to look for (e.g. 0 is the first in the field's array, 1 is the second, and so on)
  */
static inline int64 UMGetInt64(const UMessage * msg, const char * fieldName, uint32 idx) {int64 r; return (UMFindInt64(msg, fieldName, idx, &r)==CB_NO_ERROR)?r:0;}

/** Queries the UMessage for a particular floating point value.
  * @param msg The UMessage to query.
  * @param fieldName The field to name to look inside
  * @param idx The index of the floating point item to look for (e.g. 0 is the first in the array, 1 is the second, and so on)
  * @param retFloat on success, the requested value is written into this location
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (field name not found, was of the wrong type, or the array was shorter than (idx+1) items)
  */
c_status_t UMFindFloat(const UMessage * msg, const char * fieldName, uint32 idx, float * retFloat);

/** Returns a pointer to the requested floating point value, or a default value (0.0f) on failure.
  * @param msg The UMessage to query.
  * @param fieldName The field name to look inside.
  * @param idx The index of the floating point value to look for (e.g. 0 is the first in the field's array, 1 is the second, and so on)
  */
static inline float UMGetFloat(const UMessage * msg, const char * fieldName, uint32 idx) {float r; return (UMFindFloat(msg, fieldName, idx, &r)==CB_NO_ERROR)?r:0.0f;}

/** Queries the UMessage for a particular double-precision floating point value.
  * @param msg The UMessage to query.
  * @param fieldName The field to name to look inside
  * @param idx The index of the double-precision floating point item to look for (e.g. 0 is the first in the array, 1 is the second, and so on)
  * @param retDouble on success, the requested value is written into this location
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (field name not found, was of the wrong type, or the array was shorter than (idx+1) items)
  */
c_status_t UMFindDouble(const UMessage * msg, const char * fieldName, uint32 idx, double * retDouble);

/** Returns a pointer to the requested double-precision floating point value, or a default value (0.0) on failure.
  * @param msg The UMessage to query.
  * @param fieldName The field name to look inside.
  * @param idx The index of the double-precision floating point to look for (e.g. 0 is the first in the field's array, 1 is the second, and so on)
  */
static inline double UMGetDouble(const UMessage * msg, const char * fieldName, uint32 idx) {double r; return (UMFindDouble(msg, fieldName, idx, &r)==CB_NO_ERROR)?r:0.0f;}

/** Queries the UMessage for a particular UPoint value.
  * @param msg The UMessage to query.
  * @param fieldName The field to name to look inside
  * @param idx The index of the UPoint item to look for (e.g. 0 is the first in the array, 1 is the second, and so on)
  * @param retPoint on success, the requested value is written into this location
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (field name not found, was of the wrong type, or the array was shorter than (idx+1) items)
  */
c_status_t UMFindPoint(const UMessage * msg, const char * fieldName, uint32 idx, UPoint * retPoint);

/** Returns a pointer to the requested UPoint value, or a default value (0.0f,0.0f) on failure.
  * @param msg The UMessage to query.
  * @param fieldName The field name to look inside.
  * @param idx The index of the UPoint to look for (e.g. 0 is the first in the field's array, 1 is the second, and so on)
  */
static inline UPoint UMGetPoint(const UMessage * msg, const char * fieldName, uint32 idx) {UPoint r; if (UMFindPoint(msg, fieldName, idx, &r)==CB_NO_ERROR) return r; else {UPoint x = {0.0f,0.0f}; return x;}}

/** Queries the UMessage for a particular URect value.
  * @param msg The UMessage to query.
  * @param fieldName The field to name to look inside
  * @param idx The index of the URect item to look for (e.g. 0 is the first in the array, 1 is the second, and so on)
  * @param retRect on success, the requested value is written into this location
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (field name not found, was of the wrong type, or the array was shorter than (idx+1) items)
  */
c_status_t UMFindRect(const UMessage * msg, const char * fieldName, uint32 idx, URect * retRect);

/** Returns a pointer to the requested URect value, or a default value (0.0f,0.0f,0.0f,0.0f) on failure.
  * @param msg The UMessage to query.
  * @param fieldName The field name to look inside.
  * @param idx The index of the URect to look for (e.g. 0 is the first in the field's array, 1 is the second, and so on)
  */
static inline URect UMGetRect(const UMessage * msg, const char * fieldName, uint32 idx) {URect r; if (UMFindRect(msg, fieldName, idx, &r)==CB_NO_ERROR) return r; else {URect x = {0.0f,0.0f,0.0f,0.0f}; return x;}}

/** Returns a pointer to the requested string value, or NULL on failure.
  * @param msg The UMessage to query.
  * @param fieldName The field name to look inside.
  * @param idx The index of the string to look for (e.g. 0 is the first in the field's array, 1 is the second, and so on)
  */
const char * UMGetString(const UMessage * msg, const char * fieldName, uint32 idx);

/** Queries the UMessage for a particular string value.
  * @param msg The UMessage to query.
  * @param fieldName The field to name to look inside
  * @param idx The index of the string item to look for (e.g. 0 is the first in the array, 1 is the second, and so on)
  * @param retStringPointer on success, a pointer to the requested string is written to this location.
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (field name not found, was of the wrong type, or the array was shorter than (idx+1) items)
  */
static inline c_status_t UMFindString(const UMessage * msg, const char * fieldName, uint32 idx, const char ** retStringPointer) {*retStringPointer = UMGetString(msg, fieldName, idx); return (*retStringPointer) ? CB_NO_ERROR : CB_ERROR;}

/** Queries the UMessage for a particular raw-data-blob.
  * @param msg The UMessage to query.
  * @param fieldName The field to name to look inside
  * @param dataType The type-code to require.  If B_ANY_TYPE is passed, then the field's type code will be ignored; otherwise, this call will only
  *                 succeed if the field's type code is equal to this.
  * @param idx The index of the raw-data-blob to look for (e.g. 0 is the first in the array, 1 is the second, and so on)
  * @param retDataBytes on success, a pointer to the requested data bytes is written to this location.
  * @param retNumBytes on the number of data bytes pointed to by (retDataBytes) is written to this location.
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (field name not found, was of the wrong type, or the array was shorter than (idx+1) items)
  */
c_status_t UMFindData(const UMessage * msg, const char * fieldName, uint32 dataType, uint32 idx, const void ** retDataBytes, uint32 * retNumBytes);

/** Queries the UMessage for a particular boolean value.
  * @param msg The UMessage to query.
  * @param fieldName The field to name to look inside
  * @param idx The index of the boolean item to look for (e.g. 0 is the first in the array, 1 is the second, and so on)
  * @param retMessage on success, the requested UMessage value is written to this location.  (Note that this is a lightweight operation, because
  *                   the returned UMessage object merely points to data within (msg)'s own data buffer; no actual message-data is copied)
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure (field name not found, was of the wrong type, or the array was shorter than (idx+1) items)
  */
c_status_t UMFindMessage(const UMessage * msg, const char * fieldName, uint32 idx, UMessage * retMessage);

/** Returns a pointer to the requested UMessage value, or an invalid UMessage value on failure.
  * @param msg The UMessage to query.
  * @param fieldName The field name to look inside.
  * @param idx The index of the UMessage to look for (e.g. 0 is the first in the field's array, 1 is the second, and so on)
  * @note You can tell if the returned UMessage is invalid by calling UMGetFlattenedSize() on it -- if the result is zero, it's an invalid UMessage.
  */
static inline UMessage UMGetMessage(const UMessage * msg, const char * fieldName, uint32 idx) {UMessage r; if (UMFindMessage(msg, fieldName, idx, &r) == CB_NO_ERROR) return r; else {UMessage x; memset(&x,0,sizeof(x)); return x;}}

/** By default, the MicroMessage API will check (when adding a new field to a Message) to make sure that no other fields with the
  * same name already exist in the message.  It does this check because Message field names are required to be unique within the
  * Message they are directly a part of, and other Message implementations do not support a Message that contains multiple different
  * fields with the same name.  However, this check can be inefficient in Messages with many fields, as doing this check is an  
  * O(N) operation, so you can call this method to disable the check.  Note that you are still responsible for making sure that
  * no duplicate fields exist, this only disables the check to verify that.
  * @param enforce New value for the global enforce-field-name-uniqueness flag.  Default value of this flag is true.
  */
void SetFieldNameUniquenessEnforced(UBool enforce);

/** Returns true iff field-name-uniqueness is being enforced.  Default value of this flag is true. */
UBool IsFieldNameUniquenessEnforced();

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
typedef struct imp_GenericArrayHandle {
   const uint32 _numItems;
   const uint8 * _itemData;
} GenericArrayHandle;

#define DECLARE_MESSAGE_DATA_ARRAY_HANDLE(typeName) typedef struct _imp_##typeName##ArrayHandle {GenericArrayHandle _baseHandle;} typeName##ArrayHandle
DECLARE_MESSAGE_DATA_ARRAY_HANDLE(URect);
DECLARE_MESSAGE_DATA_ARRAY_HANDLE(UPoint);
DECLARE_MESSAGE_DATA_ARRAY_HANDLE(Double);
DECLARE_MESSAGE_DATA_ARRAY_HANDLE(Float);
DECLARE_MESSAGE_DATA_ARRAY_HANDLE(Int64);
DECLARE_MESSAGE_DATA_ARRAY_HANDLE(Int32);
DECLARE_MESSAGE_DATA_ARRAY_HANDLE(Int16);
DECLARE_MESSAGE_DATA_ARRAY_HANDLE(Int8);
DECLARE_MESSAGE_DATA_ARRAY_HANDLE(UBool);
#endif

/** Given a (Blah)ArrayHandle object, returns the number of data-items that handle points to.
  * @param arrayHandle an (Blah)ArrayHandle object of any type (as previously returned by a UMGet*s() function call)
  */
#define UMGetNumItemsInArray(arrayHandle) ((arrayHandle)._baseHandle._numItems)

/** Returns a handle to the array of UBool values stored under the given field name.
  * @param msg The UMessage to retrieve the array from.
  * @param fieldName The field name of the desired data.
  * @returns a handle to the requested array.  On failure, UMGetNumItemsInArray()
  *          will return zero when this handle is passed to it as an argument.
  *          Items in the array can be retrieved by calling UMGetBoolFromArray().
  */
UBoolArrayHandle UMGetBools(const UMessage * msg, const char * fieldName);

/** Returns a handle to the array of int8 values stored under the given field name.
  * @param msg The UMessage to retrieve the array from.
  * @param fieldName The field name of the desired data.
  * @returns a handle to the requested array.  On failure, UMGetNumItemsInArray()
  *          will return zero when this handle is passed to it as an argument.
  *          Items in the array can be retrieved by calling UMGetInt8FromArray().
  */
Int8ArrayHandle UMGetInt8s(const UMessage * msg, const char * fieldName);

/** Returns a handle to the array of int16 values stored under the given field name.
  * @param msg The UMessage to retrieve the array from.
  * @param fieldName The field name of the desired data.
  * @returns a handle to the requested array.  On failure, UMGetNumItemsInArray()
  *          will return zero when this handle is passed to it as an argument.
  *          Items in the array can be retrieved by calling UMGetInt16FromArray().
  */
Int16ArrayHandle UMGetInt16s(const UMessage * msg, const char * fieldName);

/** Returns a handle to the array of int32 values stored under the given field name.
  * @param msg The UMessage to retrieve the array from.
  * @param fieldName The field name of the desired data.
  * @returns a handle to the requested array.  On failure, UMGetNumItemsInArray()
  *          will return zero when this handle is passed to it as an argument.
  *          Items in the array can be retrieved by calling UMGetInt32FromArray().
  */
Int32ArrayHandle UMGetInt32s(const UMessage * msg, const char * fieldName);

/** Returns a handle to the array of int64 values stored under the given field name.
  * @param msg The UMessage to retrieve the array from.
  * @param fieldName The field name of the desired data.
  * @returns a handle to the requested array.  On failure, UMGetNumItemsInArray()
  *          will return zero when this handle is passed to it as an argument.
  *          Items in the array can be retrieved by calling UMGetInt64FromArray().
  */
Int64ArrayHandle UMGetInt64s(const UMessage * msg, const char * fieldName);

/** Returns a handle to the array of float values stored under the given field name.
  * @param msg The UMessage to retrieve the array from.
  * @param fieldName The field name of the desired data.
  * @returns a handle to the requested array.  On failure, UMGetNumItemsInArray()
  *          will return zero when this handle is passed to it as an argument.
  *          Items in the array can be retrieved by calling UMGetFloatFromArray().
  */
FloatArrayHandle UMGetFloats(const UMessage * msg, const char * fieldName);

/** Returns a handle to the array of double values stored under the given field name.
  * @param msg The UMessage to retrieve the array from.
  * @param fieldName The field name of the desired data.
  * @returns a handle to the requested array.  On failure, UMGetNumItemsInArray()
  *          will return zero when this handle is passed to it as an argument.
  *          Items in the array can be retrieved by calling UMGetDoubleFromArray().
  */
DoubleArrayHandle UMGetDoubles(const UMessage * msg, const char * fieldName);

/** Returns a handle to the array of UPoint values stored under the given field name.
  * @param msg The UMessage to retrieve the array from.
  * @param fieldName The field name of the desired data.
  * @returns a handle to the requested array.  On failure, UMGetNumItemsInArray()
  *          will return zero when this handle is passed to it as an argument.
  *          Items in the array can be retrieved by calling UMGetPointFromArray().
  */
UPointArrayHandle UMGetPoints(const UMessage * msg, const char * fieldName);

/** Returns a handle to the array of URect values stored under the given field name.
  * @param msg The UMessage to retrieve the array from.
  * @param fieldName The field name of the desired data.
  * @returns a handle to the requested array.  On failure, UMGetNumItemsInArray()
  *          will return zero when this handle is passed to it as an argument.
  *          Items in the array can be retrieved by calling UMGetRectFromArray().
  */
URectArrayHandle UMGetRects(const UMessage * msg, const char * fieldName);

/** Given a UBoolArrayHandle, returns the nth UBool in the array.
  * @param handle a handle to the array, as previously returned by UMGetPoints()
  * @param idx Index of the item to be returned.
  * @returns the nth UBool in the array, or false if (idx) isn't valid.
  */
UBool UMGetBoolFromArray(UBoolArrayHandle handle, uint32 idx);

/** Given a Int8ArrayHandle, returns the nth int8 in the array.
  * @param handle a handle to the array, as previously returned by UMGetPoints()
  * @param idx Index of the item to be returned.
  * @returns the nth int8 in the array, or 0 if (idx) isn't valid.
  */
int8 UMGetInt8FromArray(Int8ArrayHandle handle, uint32 idx);

/** Given a Int16ArrayHandle, returns the nth int16 in the array.
  * @param handle a handle to the array, as previously returned by UMGetPoints()
  * @param idx Index of the item to be returned.
  * @returns the nth int16 in the array, or an all-zero Point if (idx) isn't valid.
  */
int16 UMGetInt16FromArray(Int16ArrayHandle handle, uint32 idx);

/** Given a Int32ArrayHandle, returns the nth int32 in the array.
  * @param handle a handle to the array, as previously returned by UMGetPoints()
  * @param idx Index of the item to be returned.
  * @returns the nth int32 in the array, or an all-zero Point if (idx) isn't valid.
  */
int32 UMGetInt32FromArray(Int32ArrayHandle handle, uint32 idx);

/** Given a Int64ArrayHandle, returns the nth int64 in the array.
  * @param handle a handle to the array, as previously returned by UMGetPoints()
  * @param idx Index of the item to be returned.
  * @returns the nth int64 in the array, or an all-zero Point if (idx) isn't valid.
  */
int64 UMGetInt64FromArray(Int64ArrayHandle handle, uint32 idx);

/** Given a FloatArrayHandle, returns the nth float in the array.
  * @param handle a handle to the array, as previously returned by UMGetPoints()
  * @param idx Index of the item to be returned.
  * @returns the nth float in the array, or an all-zero Point if (idx) isn't valid.
  */
float UMGetFloatFromArray(FloatArrayHandle handle, uint32 idx);

/** Given a DoubleArrayHandle, returns the nth double in the array.
  * @param handle a handle to the array, as previously returned by UMGetPoints()
  * @param idx Index of the item to be returned.
  * @returns the nth double in the array, or an all-zero Point if (idx) isn't valid.
  */
double UMGetDoubleFromArray(DoubleArrayHandle handle, uint32 idx);

/** Given a UPointArrayHandle, returns the nth UPoint in the array.
  * @param handle a handle to the array, as previously returned by UMGetPoints()
  * @param idx Index of the item to be returned.
  * @returns the nth UPoint in the array, or an all-zero Point if (idx) isn't valid.
  */
UPoint UMGetPointFromArray(UPointArrayHandle handle, uint32 idx);

/** Given a URectArrayHandle, returns the nth URect in the array.
  * @param handle a handle to the array, as previously returned by UMGetRects()
  * @param idx Index of the URect to be returned.
  * @returns the nth URect in the array, or an all-zero URect if (idx) isn't valid.
  */
URect UMGetRectFromArray(URectArrayHandle handle, uint32 idx);

/** @} */ // end of micromessage doxygen group

#ifdef __cplusplus
};
#endif

#endif

