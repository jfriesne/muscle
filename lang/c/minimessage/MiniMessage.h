#ifndef MiniMessage_h
#define MiniMessage_h

#include "support/MuscleSupport.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup minimessage The MiniMessage C function API 
 *  These functions are all defined in MiniMessage(.c,.h), and are stand-alone
 *  C functions that provide a way for C programs to use MUSCLE Messages.
 *  This is a minimalist implentation and not so easy to use as the C++ Message
 *  class, but on the plus side it is much more space-efficient, compiling
 *  to just over 80KB of code.
 *
 *  @note This implementation employs dynamic memory-allocation internally,
 *        and as such is potentially subject to memory leaks or heap fragmentation.
 *        If you're looking for a super-lightweight implementation that never
 *        uses the heap at all, check out the \ref micromessage, in
 *        the muscle/micromessage folder.
 *  @{
 */

/** My own little boolean type, since C doesn't come with one built in. */
typedef char MBool;

/** Supported values for the MBool type */
enum {
   MFalse = 0, /**< Constant value for boolean-false (zero) */
   MTrue       /**< Constant value for boolean-true (one)   */
};

/* This file contains a C API for a "minimalist" implementation of the MUSCLE  */
/* Message dictionary object.  This implementation sacrifices a certain amount */
/* of flexibility and convenience in exchange for a very lightweight           */
/* and efficient implementation.                                               */

/** Definition of our Point class -- two floats */
typedef struct _MPoint {
   float x;  /**< horizontal axis co-ordinate */
   float y;  /**< vertical axis co-ordinate   */
} MPoint;

/** Definition of our Rect class -- four floats */
typedef struct _MRect {
   float left;   /**< left edge of the rectangle */
   float top;    /**< top edge of the rectangle */
   float right;  /**< right edge of the rectangle */
   float bottom; /**< bottom edge of the rectangle */
} MRect;

struct _MMessage;

/** Definition of our opaque handle to a MMessage object.  Your
  * code doesn't know what a (MMessage *) points to, and it doesn't care,
  * because all operations on it should happen via calls to the functions
  * that are defined below.
  */
typedef struct _MMessage MMessage;

/** This object is used in field name iterations */
typedef struct _MMessageIterator {
   const MMessage * message;  /**< Message whose fields we are currently iterating over */
   void * iterState;          /**< Internal implementation detail, please ignore */
   uint32 typeCode;           /**< type code we are looking for, or B_ANY_TYPE if any type is okay */  
} MMessageIterator;

/** Definition of our byte-array class, including size value */
typedef struct _MByteBuffer {
   uint32 numBytes;  /**< Number of bytes allocated in this struct */
   uint8 bytes;      /**< Note that this is only the first byte; there are usually more after this one */
} MByteBuffer;

/** Allocates and initializes a new MByteBuffer with the specified number of bytes, and returns a pointer to it. 
  * @param numBytes How many bytes to allocate in this buffer.
  * @param clearBytes If MTrue, all the data bytes in the returned MByteBuffer will be zero.
  *                   If MFalse, the bytes' values will be undefined (which is a bit more efficient)
  * @returns a newly allocated MByteBuffer with the specified number of bytes, or NULL on failure.  
  *          If non-NULL, it becomes the responsibility of the calling code to call FreeMByteBuffer() 
  *          on the MByteBuffer when it is done using it. 
  */
MByteBuffer * MBAllocByteBuffer(uint32 numBytes, MBool clearBytes);

/** Allocates and initializes a new MByteBuffer to contain a copy of the specified NUL-terminated string.
  * @param sourceString The string to copy into the newly allocated byte buffer.
  * @returns A newly allocated MByteBuffer containing a copy of the specified string, or NULL on failure.
  *          Note that the returned MByteBuffer's string will be NUL-terminated too.
  */
MByteBuffer * MBStrdupByteBuffer(const char * sourceString);

/** Attempts to create and return a cloned copy of (cloneMe).
  * @param cloneMe The MByteBuffer to create a copy of.  
  * @returns The newly allocated MByteBuffer, or NULL on failure.
  */
MByteBuffer * MBCloneByteBuffer(const MByteBuffer * cloneMe);

/** Returns MTrue iff the two byte buffers are equal (i.e. both hold the same byte sequence)
  * @param buf1 The first byte buffer to compare.
  * @param buf2 The first byte buffer to compare.
  * @returns True if buf1 has the same byte sequence as buf2, otherwise MFalse.
  */
MBool MBAreByteBuffersEqual(const MByteBuffer * buf1, const MByteBuffer * buf2);

/** Frees a previously created MByteBuffer and all the data that it holds.
  * @param msg The MByteBuffer to free.  If NULL, no action will be taken.
  */
void MBFreeByteBuffer(MByteBuffer * msg);

/** Allocates and initializes a new MMessage with the specified what code, and returns a pointer to it. 
  * @param what Initial 'what' code to give to the MMessage.
  * @returns a newly allocated MMessage, or NULL on failure.  If non-NULL, it becomes the
  *          the responsibility of the calling code to call MMFreeMessage() on the MMessage
  *          when it is done using it. 
  */
MMessage * MMAllocMessage(uint32 what);

/** Attempts to create and return a cloned copy of (cloneMe).
  * @param cloneMe The MMessage to create a copy of.  
  * @returns The newly allocated MMessage, or NULL on failure.
  */
MMessage * MMCloneMessage(const MMessage * cloneMe);

/** Frees a previously created MMessage and all the data that it holds.
  * @param msg The MMessage to free.  If NULL, no action will be taken.
  */
void MMFreeMessage(MMessage * msg);

/** Returns the 'what' code associated with the specified MMessage. 
  * @param msg The MMessage to retrieve the 'what' code of.'
  * @returns The MMessage's what code.
  */
uint32 MMGetWhat(const MMessage * msg);

/** Sets the 'what' code associated with the specified MMessage. 
  * @param msg The MMessage to set the 'what' code of'.
  * @param newWhat The new 'what' code to install.  (Typically a B_*_TYPE value)
  */
void MMSetWhat(MMessage * msg, uint32 newWhat);

/** Removes and frees all of the supplied MMessage's field data.  The MMessage itself
  * is not destroyed, however (use MMFreeMessage() to do that)
  * @param msg The MMessage object to remove all data fields from.
  */
void MMClearMessage(MMessage * msg);

/** Attempts to remove and free the specified field from the given MMessage.
  * @param msg the MMessage object to remove the field from
  * @param fieldName Name of the field to remove and free. 
  * @returns CB_NO_ERROR if the field was found and removed, or CB_ERROR if it wasn't found.
  */
c_status_t MMRemoveField(MMessage * msg, const char * fieldName);

/** Attempts to create and install a string field with the specified field name into the MMessage.
  * On success, any previously installed field with the same name will be replaced and freed.
  * @param msg The MMessage to install the new field into.
  * @param retainOldData This flag is relevant only if a string field with the same name already exists.
  *                      If MTrue, as many of the old field's data values as possible will be transferred
  *                      to the new field.  Otherwise, all the old field's data will be destroyed and the
  *                      new field will be created with all NULL string values.
  * @param fieldName Field name of the new field.
  * @param numItems Number of items that the new field should contain room for.
  * @returns A pointer to an array of (numItems) MByteBuffer pointers, or NULL if there was an error.
  *          The returned array belongs to the MMessage, and will be freed by it at the proper time.
  *          The MByteBuffer pointers in the array, when non-NULL, are also considered to belong to the
  *          MMessage, and will have MBFreeByteBuffer() called on them when the field is destroyed.  
  *          NOTE: When setting a value in the array, be sure to allocate its memory using MBAllocByteBuffer()
  *                or MBStrdupByteBuffer(), and be sure to call MBFreeByteBuffer() on the old value before
  *                you replace it.
  */
MByteBuffer ** MMPutStringField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems);

/** Attempts to create and install a boolean field with the specified field name into the MMessage.
  * On success, any previously installed field with the same name will be replaced and freed.
  * @param msg The MMessage to install the new field into.
  * @param retainOldData This flag is relevant only if a boolean field with the same name already exists.
  *                      If MTrue, as many of the old field's data values as possible will be transferred
  *                      to the new field.  Otherwise, all the old field's data will be destroyed and the
  *                      new field will be created with all data values set to MFalse.
  * @param fieldName Field name of the new field.
  * @param numItems Number of items that the new field should contain room for.
  * @returns A pointer to an array of (numItems) booleans, or NULL if there was an error.
  *          The returned array belongs to the MMessage, and will be freed by it at the proper time.
  */
MBool * MMPutBoolField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems);

/** Attempts to create and install an int8 field with the specified field name into the MMessage.
  * On success, any previously installed field with the same name will be replaced and freed.
  * @param msg The MMessage to install the new field into.
  * @param retainOldData This flag is relevant only if an int8 field with the same name already exists.
  *                      If MTrue, as many of the old field's data values as possible will be transferred
  *                      to the new field.  Otherwise, all the old field's data will be destroyed and the
  *                      new field will be created with all zero data values.
  * @param fieldName Field name of the new field.
  * @param numItems Number of items that the new field should contain room for.
  * @returns A pointer to an array of (numItems) int8s, or NULL if there was an error.
  *          The returned array belongs to the MMessage, and will be freed by it at the proper time.
  */
int8 * MMPutInt8Field(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems);

/** Attempts to create and install an int16 field with the specified field name into the MMessage.
  * On success, any previously installed field with the same name will be replaced and freed.
  * @param msg The MMessage to install the new field into.
  * @param retainOldData This flag is relevant only if an int16 field with the same name already exists.
  *                      If MTrue, as many of the old field's data values as possible will be transferred
  *                      to the new field.  Otherwise, all the old field's data will be destroyed and the
  *                      new field will be created with all zero data values.
  * @param fieldName Field name of the new field.
  * @param numItems Number of items that the new field should contain room for.
  * @returns A pointer to an array of (numItems) int16s, or NULL if there was an error.
  *          The returned array belongs to the MMessage, and will be freed by it at the proper time.
  */
int16 * MMPutInt16Field(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems);

/** Attempts to create and install an int32 field with the specified field name into the MMessage.
  * On success, any previously installed field with the same name will be replaced and freed.
  * @param msg The MMessage to install the new field into.
  * @param retainOldData This flag is relevant only if an int32 field with the same name already exists.
  *                      If MTrue, as many of the old field's data values as possible will be transferred
  *                      to the new field.  Otherwise, all the old field's data will be destroyed and the
  *                      new field will be created with all zero data values.
  * @param fieldName Field name of the new field.
  * @param numItems Number of items that the new field should contain room for.
  * @returns A pointer to an array of (numItems) int32s, or NULL if there was an error.
  *          The returned array belongs to the MMessage, and will be freed by it at the proper time.
  */
int32 * MMPutInt32Field(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems);

/** Attempts to create and install an int64 field with the specified field name into the MMessage.
  * On success, any previously installed field with the same name will be replaced and freed.
  * @param msg The MMessage to install the new field into.
  * @param retainOldData This flag is relevant only if an int64 field with the same name already exists.
  *                      If MTrue, as many of the old field's data values as possible will be transferred
  *                      to the new field.  Otherwise, all the old field's data will be destroyed and the
  *                      new field will be created with all zero data values.
  * @param fieldName Field name of the new field.
  * @param numItems Number of items that the new field should contain room for.
  * @returns A pointer to an array of (numItems) int64s, or NULL if there was an error.
  *          The returned array belongs to the MMessage, and will be freed by it at the proper time.
  */
int64 * MMPutInt64Field(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems);

/** Attempts to create and install a float field with the specified field name into the MMessage.
  * On success, any previously installed field with the same name will be replaced and freed.
  * @param msg The MMessage to install the new field into.
  * @param retainOldData This flag is relevant only if a float field with the same name already exists.
  *                      If MTrue, as many of the old field's data values as possible will be transferred
  *                      to the new field.  Otherwise, all the old field's data will be destroyed and the
  *                      new field will be created with all zero data values.
  * @param fieldName Field name of the new field.
  * @param numItems Number of items that the new field should contain room for.
  * @returns A pointer to an array of (numItems) floats, or NULL if there was an error.
  *          The returned array belongs to the MMessage, and will be freed by it at the proper time.
  */
float * MMPutFloatField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems);

/** Attempts to create and install a double field with the specified field name into the MMessage.
  * On success, any previously installed field with the same name will be replaced and freed.
  * @param msg The MMessage to install the new field into.
  * @param retainOldData This flag is relevant only if a double field with the same name already exists.
  *                      If MTrue, as many of the old field's data values as possible will be transferred
  *                      to the new field.  Otherwise, all the old field's data will be destroyed and the
  *                      new field will be created with all zero data values.
  * @param fieldName Field name of the new field.
  * @param numItems Number of items that the new field should contain room for.
  * @returns A pointer to an array of (numItems) doubles, or NULL if there was an error.
  *          The returned array belongs to the MMessage, and will be freed by it at the proper time.
  */
double * MMPutDoubleField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems);

/** Attempts to create and install a Message field with the specified field name into the MMessage.
  * On success, any previously installed field with the same name will be replaced and freed.
  * @param msg The MMessage to install the new field into.
  * @param retainOldData This flag is relevant only if a Message field with the same name already exists.
  *                      If MTrue, as many of the old field's data values as possible will be transferred
  *                      to the new field.  Otherwise, all the old field's data will be destroyed and the
  *                      new field will be created with all NULL data values.
  * @param fieldName Field name of the new field.
  * @param numItems Number of items that the new field should contain room for.
  * @returns A pointer to an array of (numItems) Messages, or NULL if there was an error.
  *          The returned array belongs to the MMessage, and will be freed by it at the proper time.
  *          NOTE:  Any MMessages that this array points to are considered to be owned by the MMessage
  *                 as well, for as long as they are pointed to by the array.  So when setting the array
  *                 values, be sure to follow pointer semantics, such that any new values are allocated
  *                 off the heap, and be sure to call MMFreeMessage() on any values you replace.
  */
MMessage ** MMPutMessageField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems);

/** Attempts to create and install a pointer field with the specified field name into the MMessage.
  * On success, any previously installed field with the same name will be replaced and freed.
  * @param msg The MMessage to install the new field into.
  * @param retainOldData This flag is relevant only if a pointer field with the same name already exists.
  *                      If MTrue, as many of the old field's data values as possible will be transferred
  *                      to the new field.  Otherwise, all the old field's data will be destroyed and the
  *                      new field will be created with all NULL data values.
  * @param fieldName Field name of the new field.
  * @param numItems Number of items that the new field should contain room for.
  * @returns A pointer to an array of (numItems) pointers, or NULL if there was an error.
  *          The returned array belongs to the MMessage, and will be freed by it at the proper time.
  */
void ** MMPutPointerField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems);

/** Attempts to create and install a point field with the specified field name into the MMessage.
  * On success, any previously installed field with the same name will be replaced and freed.
  * @param msg The MMessage to install the new field into.
  * @param retainOldData This flag is relevant only if a point field with the same name already exists.
  *                      If MTrue, as many of the old field's data values as possible will be transferred
  *                      to the new field.  Otherwise, all the old field's data will be destroyed and the
  *                      new field will be created with all zero data values.
  * @param fieldName Field name of the new field.
  * @param numItems Number of items that the new field should contain room for.
  * @returns A pointer to an array of (numItems) points, or NULL if there was an error.
  *          The returned array belongs to the MMessage, and will be freed by it at the proper time.
  */
MPoint * MMPutPointField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems);

/** Attempts to create and install a rect field with the specified field name into the MMessage.
  * On success, any previously installed field with the same name will be replaced and freed.
  * @param msg The MMessage to install the new field into.
  * @param retainOldData This flag is relevant only if a rect field with the same name already exists.
  *                      If MTrue, as many of the old field's data values as possible will be transferred
  *                      to the new field.  Otherwise, all the old field's data will be destroyed and the
  *                      new field will be created with all zero data values.
  * @param fieldName Field name of the new field.
  * @param numItems Number of items that the new field should contain room for.
  * @returns A pointer to an array of (numItems) rects, or NULL if there was an error.
  *          The returned array belongs to the MMessage, and will be freed by it at the proper time.
  */
MRect * MMPutRectField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems);

/** Attempts to create and install an untyped data field with the specified field name into the MMessage.
  * On success, any previously installed field with the same name will be replaced and freed.
  * @param msg The MMessage to install the new field into.
  * @param retainOldData This flag is relevant only if a data field with the same name already exists.
  *                      If MTrue, as many of the old field's data values as possible will be transferred
  *                      to the new field.  Otherwise, all the old field's data will be destroyed and the
  *                      new field will be created with all NULL string values.
  * @param typeCode The type code of the data field to place.
  * @param fieldName Field name of the new field.
  * @param numItems Number of items that the new field should contain room for.
  * @returns A pointer to an array of (numItems) MByteBuffer pointers, or NULL if there was an error.
  *          The returned array belongs to the MMessage, and will be freed by it at the proper time.
  *          The MByteBuffer pointers in the array, when non-NULL, are also considered to belong to the
  *          MMessage, and will have MBFreeByteBuffer() called on them when the field is destroyed.  
  *          NOTE: When setting a value in the array, be sure to allocate its memory using MBAllocByteBuffer()
  *                or MBStrdupByteBuffer(), and be sure to call MBFreeByteBuffer() on the old value before
  *                you replace it.
  */
MByteBuffer ** MMPutDataField(MMessage * msg, MBool retainOldData, uint32 typeCode, const char * fieldName, uint32 numItems);

/** Returns the number of bytes it would take to hold a flattened representation of (msg). 
  * @param msg MMessage to scan to determine its flattened size.
  * @returns Number of bytes the flattened representation would require.
  */
uint32 MMGetFlattenedSize(const MMessage * msg);

/** Flattens the supplied MMessage into a platform-neutral byte buffer that can be sent out over the
  * network or saved to disk and later reassembled back into an equivalent MMessage object by calling
  * MMUnflattenMessage().
  * @param msg MMessage to produce the byte buffer from.
  * @param outBuf Pointer to the output array to write into.  There must be at least
  *               MMGetFlattenedSize(msg) bytes of storage available at this location,
  *               or memory corruption will result.  The output into this buffer will be in
  *               with the standard MUSCLE flattened byte buffer format.
  */
void MMFlattenMessage(const MMessage * msg, void * outBuf);

/** Unflattens the supplied byte buffer into the supplied MMessage object.
  * @param msg MMessage object to set the state of, based on the contents of the flattened byte buffer.
  * @param inBuf Buffer containing the flattened MMessage bytes, as previously created by 
  *              MMFlattenMessage() (or some other code that writes the flattened MUSCLE Message format).
  * @param bufSizeBytes How many valid bytes of data are available at (inBuf).
  * @returns CB_NO_ERROR if the restoration was a success, or CB_ERROR otherwise (in which case (msg) will
  *                     likely be left in some valid but only partially restored state)
  */
c_status_t MMUnflattenMessage(MMessage * msg, const void * inBuf, uint32 bufSizeBytes);

/** Moves the specified field from one MMessage to another.
  * @param sourceMsg The MMessage where the field currently resides.
  * @param fieldName Name of the field to move.
  * @param destMsg The MMessage to move the field to.  If a field with this name already exists
  *                inside (destMsg), it will be replaced and freed.  If (destMsg) is NULL, the
  *                field will be removed from the source Message and freed.
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure.  
  */
c_status_t MMMoveField(MMessage * sourceMsg, const char * fieldName, MMessage * destMsg);

/** Copies the specified field from one MMessage to another.
  * @param sourceMsg The MMessage where the field currently resides.
  * @param fieldName Name of the field to copy.
  * @param destMsg The MMessage to copy the field to.  If a field with this name already exists
  *                inside (destMsg), it will be replaced and freed.  If (destMsg) is NULL, this
  *                call will have no effect.
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure.  
  */
c_status_t MMCopyField(const MMessage * sourceMsg, const char * fieldName, MMessage * destMsg);

/** Change the name of a field within its Message.
  * @param sourceMsg The MMessage where the field currently resides.
  * @param oldFieldName Current name of the field.
  * @param newFieldName Desired new name of the field.  If a field with this name already exists
  *                inside (sourceMsg), it will be replaced and freed.
  * @returns CB_NO_ERROR on success, or CB_ERROR on failure.  
  */
c_status_t MMRenameField(MMessage * sourceMsg, const char * oldFieldName, const char * newFieldName);

/** Retrieves the string field with the given name.
  * @param msg The MMessage to look for the field in.
  * @param fieldName Name of the field to look for.
  * @param optRetNumItems If non-NULL, the number of items in the field will be written into the uint32 this points to.
  * @returns A pointer to the array of MByteBuffers that represents the strings on success, or NULL on failure.
  *          Note that the returned array remains under the ownership of the MMessage object.
  */
MByteBuffer ** MMGetStringField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems);

/** Retrieves the boolean field with the given name.
  * @param msg The MMessage to look for the field in.
  * @param fieldName Name of the field to look for.
  * @param optRetNumItems If non-NULL, the number of items in the field will be written into the uint32 this points to.
  * @returns A pointer to the array of MBool on success, or NULL on failure (field not found).
  *          Note that the returned array remains under the ownership of the MMessage object.
  */
MBool * MMGetBoolField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems);

/** Retrieves the int8 field with the given name.
  * @param msg The MMessage to look for the field in.
  * @param fieldName Name of the field to look for.
  * @param optRetNumItems If non-NULL, the number of items in the field will be written into the uint32 this points to.
  * @returns A pointer to the array of int8s on success, or NULL on failure (field not found).
  *          Note that the returned array remains under the ownership of the MMessage object.
  */
int8 * MMGetInt8Field(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems);

/** Retrieves the int16 field with the given name.
  * @param msg The MMessage to look for the field in.
  * @param fieldName Name of the field to look for.
  * @param optRetNumItems If non-NULL, the number of items in the field will be written into the uint32 this points to.
  * @returns A pointer to the array of int16s on success, or NULL on failure (field not found).
  *          Note that the returned array remains under the ownership of the MMessage object.
  */
int16 * MMGetInt16Field(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems);

/** Retrieves the int32 field with the given name.
  * @param msg The MMessage to look for the field in.
  * @param fieldName Name of the field to look for.
  * @param optRetNumItems If non-NULL, the number of items in the field will be written into the uint32 this points to.
  * @returns A pointer to the array of int32s on success, or NULL on failure (field not found).
  *          Note that the returned array remains under the ownership of the MMessage object.
  */
int32 * MMGetInt32Field(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems);

/** Retrieves the int64 field with the given name.
  * @param msg The MMessage to look for the field in.
  * @param fieldName Name of the field to look for.
  * @param optRetNumItems If non-NULL, the number of items in the field will be written into the uint32 this points to.
  * @returns A pointer to the array of int64 on success, or NULL on failure (field not found).
  *          Note that the returned array remains under the ownership of the MMessage object.
  */
int64 * MMGetInt64Field(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems);

/** Retrieves the float field with the given name.
  * @param msg The MMessage to look for the field in.
  * @param fieldName Name of the field to look for.
  * @param optRetNumItems If non-NULL, the number of items in the field will be written into the uint32 this points to.
  * @returns A pointer to the array of floats on success, or NULL on failure (field not found).
  *          Note that the returned array remains under the ownership of the MMessage object.
  */
float * MMGetFloatField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems);

/** Retrieves the double field with the given name.
  * @param msg The MMessage to look for the field in.
  * @param fieldName Name of the field to look for.
  * @param optRetNumItems If non-NULL, the number of items in the field will be written into the uint32 this points to.
  * @returns A pointer to the array of doubles on success, or NULL on failure (field not found).
  *          Note that the returned array remains under the ownership of the MMessage object.
  */
double * MMGetDoubleField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems);

/** Retrieves the Message field with the given name.
  * @param msg The MMessage to look for the field in.
  * @param fieldName Name of the field to look for.
  * @param optRetNumItems If non-NULL, the number of items in the field will be written into the uint32 this points to.
  * @returns A pointer to the array of Messages on success, or NULL on failure (field not found).
  *          Note that the returned array remains under the ownership of the MMessage object.
  */
MMessage ** MMGetMessageField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems);

/** Retrieves the pointer field with the given name.
  * @param msg The MMessage to look for the field in.
  * @param fieldName Name of the field to look for.
  * @param optRetNumItems If non-NULL, the number of items in the field will be written into the uint32 this points to.
  * @returns A pointer to the array of pointers on success, or NULL on failure (field not found).
  *          Note that the returned array remains under the ownership of the MMessage object.
  */
void ** MMGetPointerField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems);

/** Retrieves the point field with the given name.
  * @param msg The MMessage to look for the field in.
  * @param fieldName Name of the field to look for.
  * @param optRetNumItems If non-NULL, the number of items in the field will be written into the uint32 this points to.
  * @returns A pointer to the array of points on success, or NULL on failure (field not found).
  *          Note that the returned array remains under the ownership of the MMessage object.
  */
MPoint * MMGetPointField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems);

/** Retrieves the rect field with the given name.
  * @param msg The MMessage to look for the field in.
  * @param fieldName Name of the field to look for.
  * @param optRetNumItems If non-NULL, the number of items in the field will be written into the uint32 this points to.
  * @returns A pointer to the array of rects on success, or NULL on failure (field not found).
  *          Note that the returned array remains under the ownership of the MMessage object.
  */
MRect * MMGetRectField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems);

/** Retrieves the data field with the given name.
  * @param msg The MMessage to look for the field in.
  * @param typeCode The typecode of the field to look for, or B_ANY_TYPE if you aren't particular about type.
  * @param fieldName Name of the field to look for.
  * @param optRetNumItems If non-NULL, the number of items in the field will be written into the uint32 this points to.
  * @returns A pointer to the array of data items on success, or NULL on failure (field not found).
  *          Note that the returned array remains under the ownership of the MMessage object.
  */
MByteBuffer ** MMGetDataField(const MMessage * msg, uint32 typeCode, const char * fieldName, uint32 * optRetNumItems);

/** Returns information about the type and size of the specified field.
  * @param msg The MMessage to look for the field in.
  * @param fieldName The name of the field to look for.
  * @param typeCode The typecode of the field to look for, or B_ANY_TYPE if you aren't particular about type.
  * @param optRetNumItems If non-NULL, on success the uint32 this points to will have the number of items in the field written into it.
  * @param optRetTypeCode If non-NULL, on success the uint32 this points to will have the type code of the field written into it.
  * @returns CB_NO_ERROR if the field was found, or CB_ERROR if no field with the specified name and type were present.
  */
c_status_t MMGetFieldInfo(const MMessage * msg, const char * fieldName, uint32 typeCode, uint32 * optRetNumItems, uint32 * optRetTypeCode);

/** Returns MTrue iff the two MMessage objects are exactly equivalent.  (Note that field ordering is not considered)
  * @param msg1 First MMessage to compare.  Must not be NULL.
  * @param msg2 Second MMessage to compare.  Must not be NULL.
  * @returns MTrue If the two MMessage objects are equal; MFalse otherwise.
  */
MBool MMAreMessagesEqual(const MMessage * msg1, const MMessage * msg2);

/** Prints the contents of this MMessage to stdout.  Useful for debugging.
  * @param msg The MMessage to print the contents of to stdout.
  * @param optFile If non-NULL, the file to print the output to.  If NULL, the output will go to stdout.
  */
void MMPrintToStream(const MMessage * msg, FILE * optFile);

/** Returns an iterator object that you can use to iterator over the field names of this MMessage.
  * @param msg The MMessage object over whose field names you wish to iterate.
  * @param typeCode The type of field you are interested in, or B_ANY_TYPE if you aren't particular.
  * @returns the iterator object, suitable, for passing in to the MMGetNextFieldName() function.
  * @note It is an error to remove or replace fields in a MMessage object while iterating over it.
  */
MMessageIterator MMGetFieldNameIterator(const MMessage * msg, uint32 typeCode);

/** Returns the next field name in the field name iteration, or NULL if there are no more field names.
  * @param iteratorPtr This should point to the iterator object that was returned by a MMGetFieldNameIterator() call.
  * @param optRetTypeCode If non-NULL, the uint32 this points to will have the next field's type code written into it.
  * @returns The next field name, or NULL if the iteration is complete.
  */
const char * MMGetNextFieldName(MMessageIterator * iteratorPtr, uint32 * optRetTypeCode);

#ifdef MUSCLE_ENABLE_MEMORY_TRACKING

/** A wrapper for malloc() that allows us to track the number of bytes currently allocated.  
  * Good for catching memory leaks.  Only enabled if MUSCLE_ENABLE_MEMORY_TRACKING is defined; otherwise defined to be equivalent to malloc().
  * @param numBytes the number of bytes to allocate
  */
void * MMalloc(uint32 numBytes);

/** A wrapper for free() that allows us to track the number of bytes currently allocated.
  * Good for catching memory leaks.  Only enabled if MUSCLE_ENABLE_MEMORY_TRACKING is defined; otherwise defined to be equivalent to free().
  * @param ptr the buffer to free (as previously allocated using MMalloc())
  */
void MFree(void * ptr);

/** A wrapper for realloc() that allows us to track the number of bytes currently allocated.
  * Good for catching memory leaks.  Only enabled if MUSCLE_ENABLE_MEMORY_TRACKING is defined; otherwise defined to be equivalent to realloc().
  * @param oldBuf a pointer to reallocate, realloc() style
  * @param newSize the desired size of the post-reallocation buffer
  * @returns a pointer to the post-reallocation buffer
  */
void * MRealloc(void * oldBuf, uint32 newSize);

#else
# define MMalloc  malloc   /**< if MUSCLE_ENABLE_MEMORY_TRACKING is not defined, then MMalloc() just becomes a synonym for malloc()   */
# define MRealloc realloc  /**< if MUSCLE_ENABLE_MEMORY_TRACKING is not defined, then MRealloc() just becomes a synonym for realloc() */
# define MFree    free     /**< if MUSCLE_ENABLE_MEMORY_TRACKING is not defined, then MFree() just becomes a synonym for free()       */
#endif

/** Returns the current number of allocated bytes. 
  * Good for catching memory leaks.  Only enabled if MUSCLE_ENABLE_MEMORY_TRACKING is defined; otherwise always returns zero.
  */
uint32 MGetNumBytesAllocated();

/** @} */ // end of minimessage doxygen group

#ifdef __cplusplus
};
#endif

#endif
