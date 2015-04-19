#include <string.h>
#include "micromessage/MicroMessage.h"

#ifdef cplusplus
extern "C" {
#endif

#define OLDEST_SUPPORTED_PROTOCOL_VERSION 1347235888 /* 'PM00' */
#define CURRENT_PROTOCOL_VERSION          1347235888 /* 'PM00' */

static UBool _enforceFieldNameUniqueness = UTrue;
void SetFieldNameUniquenessEnforced(UBool enforce) {_enforceFieldNameUniqueness = enforce;}
UBool IsFieldNameUniquenessEnforced() {return _enforceFieldNameUniqueness;}

/** Returns the number of bytes that are actually valid (i.e. written to, not garbage bytes) starting at (ptr) */
static uint32 GetNumValidBytesAt(const UMessage * msg, const uint8 * ptr)
{
   const uint8 * afterLast = msg->_buffer+msg->_numValidBytes;
   return ((ptr >= msg->_buffer)&&(ptr < afterLast)) ? (afterLast-ptr) : 0;
}

/** Returns the number of bytes that are present (i.e. in the buffer) starting at (ptr).  Note that the values of these bytes may or may not be well-defined at this time. */
static uint32 GetNumBufferBytesAt(const UMessage * msg, const uint8 * ptr)
{
   const uint8 * afterLast = msg->_buffer+msg->_bufferSize;
   return ((ptr >= msg->_buffer)&&(ptr < afterLast)) ? (afterLast-ptr) : 0;
}

static inline uint32 GetNumRemainingSpareBufferBytes(const UMessage * msg) {return GetNumBufferBytesAt(msg, msg->_buffer+msg->_numValidBytes);}

static inline void UMWriteInt32(uint8 * ptr, uint32 val) {*((uint32 *)ptr) = B_HOST_TO_LENDIAN_INT32(val);}
static inline uint32 UMReadInt32(const uint8 * ptr)      {return B_LENDIAN_TO_HOST_INT32(*((uint32 *)ptr));}

static status_t UMWriteInt32AtOffset(UMessage * msg, uint32 offset, uint32 value)
{
   uint8 * ptr = msg->_buffer+offset;
   if (GetNumBufferBytesAt(msg, ptr) < sizeof(uint32)) return B_ERROR;
   UMWriteInt32(ptr, value);
   return B_NO_ERROR;
}

static uint32 UMReadInt32AtOffset(const UMessage * msg, uint32 offset)
{
   uint8 * ptr = msg->_buffer+offset;
   return (GetNumValidBytesAt(msg,ptr) >= sizeof(uint32)) ? UMReadInt32(ptr) : 0;
}

static const uint32 MESSAGE_HEADER_SIZE = (3*sizeof(uint32));  // a Message with no fields is this long

static inline status_t UMSetNumFields(UMessage * msg, uint32 numFields) {return UMWriteInt32AtOffset(msg, 2*sizeof(uint32), numFields);}
uint32 UMGetNumFields(             const UMessage * msg) {return UMReadInt32AtOffset( msg, 2*sizeof(uint32));}
uint32 UMGetFlattenedSize(         const UMessage * msg) {return msg->_numValidBytes;}
uint32 UMGetMaximumSize(           const UMessage * msg) {return msg->_bufferSize;}
const uint8 * UMGetFlattenedBuffer(const UMessage * msg) {return msg->_buffer;}
UBool UMIsMessageReadOnly(         const UMessage * msg) {return msg->_isReadOnly;}
UBool UMIsMessageValid(            const UMessage * msg) {return ((msg->_buffer)&&(msg->_numValidBytes >= MESSAGE_HEADER_SIZE));}

status_t UMInitializeToEmptyMessage(UMessage * msg, uint8 * buf, uint32 numBytesInBuf, uint32 whatCode)
{
   if (numBytesInBuf < MESSAGE_HEADER_SIZE) return B_ERROR;

   msg->_buffer          = buf;
   msg->_bufferSize      = numBytesInBuf;
   msg->_numValidBytes   = MESSAGE_HEADER_SIZE;
   msg->_currentAddField = NULL;
   msg->_isReadOnly      = UFalse;
   msg->_parentMsg       = NULL;
   msg->_sizeField       = NULL;
   msg->_readFieldCache  = NULL;
   if ((UMWriteInt32AtOffset(msg, 0, CURRENT_PROTOCOL_VERSION) == B_NO_ERROR) &&
       (UMSetWhatCode(msg,  whatCode)                          == B_NO_ERROR) &&
       (UMSetNumFields(msg, 0)                                 == B_NO_ERROR))
   {
      return B_NO_ERROR;
   }
   else return B_ERROR;
}

status_t UMInitializeWithExistingData(UMessage * msg, const uint8 * buf, uint32 numBytesInBuf)
{
   msg->_buffer          = (uint8 *) buf;
   msg->_bufferSize      = numBytesInBuf;
   msg->_numValidBytes   = numBytesInBuf;
   msg->_currentAddField = NULL;
   msg->_isReadOnly      = UTrue;
   msg->_parentMsg       = NULL;
   msg->_sizeField       = NULL;
   msg->_readFieldCache  = NULL;
   if ((numBytesInBuf >= MESSAGE_HEADER_SIZE)&&(UMReadInt32AtOffset(msg, 0) == CURRENT_PROTOCOL_VERSION))
   {
      return B_NO_ERROR;
   }
   else return B_ERROR;
}

void UMInitializeToInvalid(UMessage * msg)
{
   msg->_buffer          = NULL;
   msg->_bufferSize      = 0;
   msg->_numValidBytes   = 0;
   msg->_currentAddField = NULL;
   msg->_isReadOnly      = UTrue;
   msg->_parentMsg       = NULL;
   msg->_sizeField       = NULL;
   msg->_readFieldCache  = NULL;
}

uint32 UMGetWhatCode(const UMessage * msg)                  {return UMReadInt32AtOffset( msg, 1*sizeof(uint32));}
status_t UMSetWhatCode(    UMessage * msg, uint32 whatCode) {return UMWriteInt32AtOffset(msg, 1*sizeof(uint32), whatCode);}

/* Per-field headers are laid out like this: */
/* 1. Field name length (4 bytes)            */
/* 2. Field name string (flattened String)   */
/* 3. Field type code (4 bytes)              */
/* 4. Field data length (4 bytes)            */
/* 5. Field data (n bytes)                   */
static inline uint32 GetFieldNameLength( uint8 * field) {return UMReadInt32(field);}
static inline const char * GetFieldName( uint8 * field) {return ((const char *)field)+sizeof(uint32);}
static inline void * GetFieldTypePointer(uint8 * field) {return (((uint8 *)GetFieldName(field))+GetFieldNameLength(field));}
static inline uint32 GetFieldType(       void * ftptr)  {return UMReadInt32((uint8 *)ftptr);}
static inline uint32 GetFieldDataLength( void * ftptr)  {return UMReadInt32(((uint8 *)ftptr)+sizeof(uint32));}
static inline uint8 * GetFieldData(      void * ftptr)  {return ((uint8 *)ftptr)+sizeof(uint32)+sizeof(uint32);}
static inline void SetFieldDataLength(   void * ftptr, uint32 newVal) {return UMWriteInt32(((uint8 *)ftptr)+sizeof(uint32), newVal);}

static const uint32 MINIMUM_FIELD_HEADERS_SIZE = (3*sizeof(uint32));  // name_length, type_code, data_length (name_string and data not included!)

static UBool IsFieldPointerValid(const UMessage * msg, uint8 * ptr)
{
   void * ftptr = (GetNumValidBytesAt(msg, ptr) >= MINIMUM_FIELD_HEADERS_SIZE) ? GetFieldTypePointer(ptr) : NULL;
   if ((ftptr)&&(GetNumValidBytesAt(msg, ((uint8*)ftptr)) >= (sizeof(uint32)+sizeof(uint32))))
   {
      uint8 * fData = GetFieldData(ftptr);
      return (GetNumValidBytesAt(msg, fData) > 0);
   }
   return UFalse;
}

static uint8 * GetNextField(UMessage * msg, uint8 * field)
{
   void * ftptr = GetFieldTypePointer(field);
   uint8 * afterData = GetFieldData(ftptr)+GetFieldDataLength(ftptr);
   return IsFieldPointerValid(msg, afterData) ? afterData : NULL;
}

static void IncreaseCurrentFieldDataLength(UMessage * msg, uint32 numBytes);  /* forward declaration */

static void IncreaseParentValidBytesBy(UMessage * msg, uint32 numBytes)
{
   /* If we are being constructed in-place inside a parent UMessage, we need to tell the parent to get larger as we get larger */
   if (msg->_parentMsg) IncreaseCurrentFieldDataLength((UMessage *)msg->_parentMsg, numBytes);
   if (msg->_sizeField) UMWriteInt32(msg->_sizeField, UMReadInt32(msg->_sizeField)+numBytes);
}

/** Note that we assume that _currentAddField is non-NULL in this method. */
static void IncreaseCurrentFieldDataLength(UMessage * msg, uint32 numBytes)
{
   msg->_numValidBytes += numBytes;
   void * ftptr = GetFieldTypePointer(msg->_currentAddField);
   SetFieldDataLength(ftptr, GetFieldDataLength(ftptr)+numBytes);
   IncreaseParentValidBytesBy(msg, numBytes);
}

static uint8 * GetFieldByNameAux(const UMessage * msg, const char * fieldName, uint32 fieldNameLength, uint32 desiredTypeCode)
{
   uint8 * ptr = msg->_buffer + MESSAGE_HEADER_SIZE;
   while(IsFieldPointerValid(msg, ptr))
   {
      void * ftptr = GetFieldTypePointer(ptr);
      if (((desiredTypeCode == B_ANY_TYPE)||(desiredTypeCode == GetFieldType(ftptr)))&&(GetFieldNameLength(ptr) == fieldNameLength)&&(strcmp(GetFieldName(ptr), fieldName) == 0)) return ptr;
      else
      {
         ptr = GetFieldData(ftptr)+GetFieldDataLength(ftptr);
      }
   }
   return NULL;
}

static uint8 * GetOrAddFieldDataPointer(UMessage * msg, const char * fieldName, uint32 fieldType, uint32 numDataBytesNeeded, uint8 ** optRetExtraFieldHeader, uint32 fieldHeaderSizeBytes)
{
   if (msg->_isReadOnly)
   {
      printf("MicroMessage Error:  Can't add data to field [%s], this MicroMessage is read-only!\n", fieldName);
      return NULL;
   }

   /** First see if this is more data for the field currently being added. */
   uint32 newFieldNameLength = ((uint32) strlen(fieldName))+1;
   if (msg->_currentAddField)
   {
      void * curFieldTypePtr = GetFieldTypePointer(msg->_currentAddField);
      if ((GetFieldType(curFieldTypePtr) == fieldType)&&(GetFieldNameLength(msg->_currentAddField) == newFieldNameLength)&&(strcmp(GetFieldName(msg->_currentAddField), fieldName) == 0))
      {
         uint8 * fieldData = GetFieldData(curFieldTypePtr);
         if (optRetExtraFieldHeader) *optRetExtraFieldHeader = fieldData;

         fieldData += GetFieldDataLength(curFieldTypePtr);
         if (GetNumBufferBytesAt(msg, fieldData) >= numDataBytesNeeded) return fieldData;
         else
         {
            printf("MicroMessage Error:  Not enough space left in "UINT32_FORMAT_SPEC"-byte buffer to append "UINT32_FORMAT_SPEC" bytes of additional data to field [%s]\n", UMGetMaximumSize(msg), numDataBytesNeeded, fieldName);
            return NULL;
         }
      }

      /** If we got here, the current field name is different from the previous one.  Optionally check to make sure such a field name doesn't already exist */
      if ((_enforceFieldNameUniqueness)&&(GetFieldByNameAux(msg, fieldName, newFieldNameLength, B_ANY_TYPE) != NULL))
      {
         printf("MicroMessage Error:  Attempt to a second field [%s] to UMessage %p, when a field with that name already exists.  This isn't supported!\n", fieldName, msg);
         return NULL;
      }
   }

   /** If we got here, we can add the new field if there is space for it. */
   {
      uint32 numRemainingBytes = GetNumRemainingSpareBufferBytes(msg);
      uint32 numRequiredBytes = sizeof(uint32)+newFieldNameLength+sizeof(uint32)+sizeof(uint32)+fieldHeaderSizeBytes+numDataBytesNeeded;
      if (numRemainingBytes < numRequiredBytes)
      {
         printf("MicroMessage Error:  Not enough space left in buffer to add new field [%s] ("UINT32_FORMAT_SPEC" additional bytes required, "UINT32_FORMAT_SPEC" bytes available)\n", fieldName, numRequiredBytes, numRemainingBytes);
         return NULL;
      }

      {
         uint32 oldValidBytes = msg->_numValidBytes;
         uint8 * ptr = msg->_currentAddField = msg->_buffer+msg->_numValidBytes;
         UMWriteInt32(ptr, newFieldNameLength);      ptr += sizeof(uint32);
         memcpy(ptr, fieldName, newFieldNameLength); ptr += newFieldNameLength;
         UMWriteInt32(ptr, fieldType);               ptr += sizeof(uint32);
         UMWriteInt32(ptr, fieldHeaderSizeBytes);    ptr += sizeof(uint32);  /* field-data-bytes not included, they will be added by caller */
         if (optRetExtraFieldHeader) *optRetExtraFieldHeader = ptr;
         memset(ptr, 0, fieldHeaderSizeBytes);       ptr += fieldHeaderSizeBytes;
         msg->_numValidBytes = ptr-msg->_buffer;
         IncreaseParentValidBytesBy(msg, msg->_numValidBytes-oldValidBytes);
         UMSetNumFields(msg, UMGetNumFields(msg)+1);
         return ptr;   /* The data bytes themselves will be added by the calling function */
      }
   }
}

status_t UMAddBools(UMessage * msg, const char * fieldName, const UBool * vals, uint32 numVals)
{
   uint8 * dataPtr = GetOrAddFieldDataPointer(msg, fieldName, B_BOOL_TYPE, numVals, NULL, 0);
   if (dataPtr == NULL) return B_ERROR;

   {
      uint32 i;
      for (i=0; i<numVals; i++) dataPtr[i] = vals[i] ? 1 : 0;
      IncreaseCurrentFieldDataLength(msg, numVals);
      return B_NO_ERROR;
   }
}

status_t UMAddInt8s(UMessage * msg, const char * fieldName, const int8 * vals, uint32 numVals)
{
   uint8 * dataPtr = GetOrAddFieldDataPointer(msg, fieldName, B_INT8_TYPE, numVals, NULL, 0);
   if (dataPtr == NULL) return B_ERROR;

   memcpy(dataPtr, vals, numVals);  // no endian-swap necessary, since these are 1-byte values anyway
   IncreaseCurrentFieldDataLength(msg, numVals);
   return B_NO_ERROR;
}

status_t UMAddInt16s(UMessage * msg, const char * fieldName, const int16 * vals, uint32 numVals)
{
   uint32 numDataBytes = numVals*sizeof(int16);
   uint8 * dataPtr = GetOrAddFieldDataPointer(msg, fieldName, B_INT16_TYPE, numDataBytes, NULL, 0);
   if (dataPtr == NULL) return B_ERROR;

   {
      int16 * d16 = (int16 *) dataPtr;
      uint32 i;
      for (i=0; i<numVals; i++) d16[i] = B_HOST_TO_LENDIAN_INT16(vals[i]);
   }
   IncreaseCurrentFieldDataLength(msg, numDataBytes);
   return B_NO_ERROR;
}

status_t UMAddInt32s(UMessage * msg, const char * fieldName, const int32 * vals, uint32 numVals)
{
   uint32 numDataBytes = numVals*sizeof(int32);
   uint8 * dataPtr = GetOrAddFieldDataPointer(msg, fieldName, B_INT32_TYPE, numDataBytes, NULL, 0);
   if (dataPtr == NULL) return B_ERROR;

   int32 * d32 = (int32 *) dataPtr;
   uint32 i;
   for (i=0; i<numVals; i++) d32[i] = B_HOST_TO_LENDIAN_INT32(vals[i]);
   IncreaseCurrentFieldDataLength(msg, numDataBytes);
   return B_NO_ERROR;
}

status_t UMAddInt64s(UMessage * msg, const char * fieldName, const int64 * vals, uint32 numVals)
{
   uint32 numDataBytes = numVals*sizeof(int64);
   uint8 * dataPtr = GetOrAddFieldDataPointer(msg, fieldName, B_INT64_TYPE, numDataBytes, NULL, 0);
   if (dataPtr == NULL) return B_ERROR;

   int64 * d64 = (int64 *) dataPtr;
   uint32 i;
   for (i=0; i<numVals; i++) d64[i] = B_HOST_TO_LENDIAN_INT64(vals[i]);
   IncreaseCurrentFieldDataLength(msg, numDataBytes);
   return B_NO_ERROR;
}

status_t UMAddFloats(UMessage * msg, const char * fieldName, const float * vals, uint32 numVals)
{
   uint32 numDataBytes = numVals*sizeof(float);
   uint8 * dataPtr = GetOrAddFieldDataPointer(msg, fieldName, B_FLOAT_TYPE, numDataBytes, NULL, 0);
   if (dataPtr == NULL) return B_ERROR;

   uint32 * dfl = (uint32 *) dataPtr;
   uint32 i;
   for (i=0; i<numVals; i++) dfl[i] = B_HOST_TO_LENDIAN_IFLOAT(vals[i]);
   IncreaseCurrentFieldDataLength(msg, numDataBytes);
   return B_NO_ERROR;
}

status_t UMAddDoubles(UMessage * msg, const char * fieldName, const double * vals, uint32 numVals)
{
   uint32 numDataBytes = numVals*sizeof(double);
   uint8 * dataPtr = GetOrAddFieldDataPointer(msg, fieldName, B_DOUBLE_TYPE, numDataBytes, NULL, 0);
   if (dataPtr == NULL) return B_ERROR;

   uint64 * ddb = (uint64 *) dataPtr;
   uint32 i;
   for (i=0; i<numVals; i++) ddb[i] = B_HOST_TO_LENDIAN_IDOUBLE(vals[i]);
   IncreaseCurrentFieldDataLength(msg, numDataBytes);
   return B_NO_ERROR;
}

status_t UMAddPoints(UMessage * msg, const char * fieldName, const UPoint * vals, uint32 numVals)
{
   uint32 numDataBytes = numVals*(sizeof(float)*2);
   uint8 * dataPtr = GetOrAddFieldDataPointer(msg, fieldName, B_POINT_TYPE, numDataBytes, NULL, 0);
   if (dataPtr == NULL) return B_ERROR;

   uint32 * dpt = (uint32 *) dataPtr;
   uint32 i;
   for (i=0; i<numVals; i++)
   {
      dpt[i*2+0] = B_HOST_TO_LENDIAN_IFLOAT(vals[i].x);
      dpt[i*2+1] = B_HOST_TO_LENDIAN_IFLOAT(vals[i].y);
   }
   IncreaseCurrentFieldDataLength(msg, numDataBytes);
   return B_NO_ERROR;
}

status_t UMAddRects(UMessage * msg, const char * fieldName, const URect * vals, uint32 numVals)
{
   uint32 numDataBytes = numVals*(sizeof(float)*4);
   uint8 * dataPtr = GetOrAddFieldDataPointer(msg, fieldName, B_RECT_TYPE, numDataBytes, NULL, 0);
   if (dataPtr == NULL) return B_ERROR;

   uint32 * drc = (uint32 *) dataPtr;
   uint32 i;
   for (i=0; i<numVals; i++)
   {
      drc[i*4+0] = B_HOST_TO_LENDIAN_IFLOAT(vals[i].left);
      drc[i*4+1] = B_HOST_TO_LENDIAN_IFLOAT(vals[i].top);
      drc[i*4+2] = B_HOST_TO_LENDIAN_IFLOAT(vals[i].right);
      drc[i*4+3] = B_HOST_TO_LENDIAN_IFLOAT(vals[i].bottom);
   }
   IncreaseCurrentFieldDataLength(msg, numDataBytes);
   return B_NO_ERROR;
}

status_t UMAddStrings(UMessage * msg, const char * fieldName, const char ** strings, uint32 numStrings)
{
   uint32 numDataBytes = numStrings*(sizeof(uint32)+1);  /* +1 is for the NUL byte that each string will have at the end */
   uint32 i;
   for (i=0; i<numStrings; i++) numDataBytes += strings[i] ? ((uint32)strlen(strings[i])) : 0;

   uint8 * numStringsHeaderPointer;
   uint8 * dataPtr = GetOrAddFieldDataPointer(msg, fieldName, B_STRING_TYPE, numDataBytes, &numStringsHeaderPointer, sizeof(uint32));
   if (dataPtr == NULL) return B_ERROR;

   UMWriteInt32(numStringsHeaderPointer, UMReadInt32(numStringsHeaderPointer)+numStrings);  /* update the number-of-strings-in-field header */
   for (i=0; i<numStrings; i++)
   {
      uint32 stringFieldLength = (strings[i]?((uint32)strlen(strings[i])):0) + 1;
      *((uint32 *)dataPtr) = B_HOST_TO_LENDIAN_INT32(stringFieldLength); dataPtr += sizeof(uint32);
      memcpy(dataPtr, strings[i]?strings[i]:"", stringFieldLength);      dataPtr += stringFieldLength;
   }
   IncreaseCurrentFieldDataLength(msg, numDataBytes);
   return B_NO_ERROR;
}

status_t UMAddData(UMessage * msg, const char * fieldName, uint32 dataType, const void * dataBytes, uint32 numBytes)
{
   uint32 numDataBytes = sizeof(uint32)+numBytes;
   uint8 * numBlobsHeaderPointer;
   uint8 * dataPtr = GetOrAddFieldDataPointer(msg, fieldName, dataType, numDataBytes, &numBlobsHeaderPointer, sizeof(uint32));
   if (dataPtr == NULL) return B_ERROR;

   UMWriteInt32(numBlobsHeaderPointer, UMReadInt32(numBlobsHeaderPointer)+1);  /* increment the number-of-blobs-in-field header */

   *((uint32 *)dataPtr) = B_HOST_TO_LENDIAN_INT32(numBytes);
   dataPtr += sizeof(uint32);

   memcpy(dataPtr, dataBytes, numBytes);
#ifdef DISABLED_TO_SHUT_CLANGS_ANALYER_UP
   dataPtr += numBytes;
#endif

   IncreaseCurrentFieldDataLength(msg, numDataBytes);
   return B_NO_ERROR;
}

status_t UMAddMessages(UMessage * msg, const char * fieldName, const UMessage * messageArray, uint32 numVals)
{
   uint32 numDataBytes = numVals*sizeof(uint32); /* for the size-header of each message */
   uint32 i;
   for (i=0; i<numVals; i++) numDataBytes += UMGetFlattenedSize(&messageArray[i]);  /* for the contents of each message */

   /* There's no number-of-messages field for this field type, for historical/compatibility reasons.  :^( */
   uint8 * dataPtr = GetOrAddFieldDataPointer(msg, fieldName, B_MESSAGE_TYPE, numDataBytes, NULL, 0);
   if (dataPtr == NULL) return B_ERROR;

   for (i=0; i<numVals; i++)
   {
      uint32 messageFieldLength = UMGetFlattenedSize(&messageArray[i]);
      *((uint32 *)dataPtr) = B_HOST_TO_LENDIAN_INT32(messageFieldLength);          dataPtr += sizeof(uint32);
      memcpy(dataPtr, UMGetFlattenedBuffer(&messageArray[i]), messageFieldLength); dataPtr += messageFieldLength;
   }
   IncreaseCurrentFieldDataLength(msg, numDataBytes);
   return B_NO_ERROR;
}

UMessage UMInlineAddMessage(UMessage * parentMsg, const char * fieldName, uint32 whatCode)
{
   UMessage ret = {0};

   uint32 numDataBytes = sizeof(uint32)+MESSAGE_HEADER_SIZE; /* sub-Message-size field, plus the initial (empty) sub-Message */

   uint8 * dataPtr = GetOrAddFieldDataPointer(parentMsg, fieldName, B_MESSAGE_TYPE, numDataBytes, NULL, 0);
   if (dataPtr == NULL)
   {
      ret._isReadOnly = true;  /* mark it as unusable for adding data to */
      return ret;
   }

   UMWriteInt32(dataPtr, MESSAGE_HEADER_SIZE);
   uint8 * childDataPtr = dataPtr+sizeof(uint32);
   (void) UMInitializeToEmptyMessage(&ret, childDataPtr, GetNumBufferBytesAt(parentMsg, childDataPtr), whatCode);  /* can't fail, we have enough bytes */
   ret._parentMsg = parentMsg;  /* So we can tell our parent UMessage to increase his size when we increase our size */
   ret._sizeField = dataPtr;    /* Remember this location, as we need to increase it when our size increases */
   IncreaseCurrentFieldDataLength(parentMsg, numDataBytes);
   return ret;
}

static UBool UMIteratorCurrentFieldMatches(UMessageFieldNameIterator * iter)
{
   return ((iter->_currentField == NULL)||(iter->_typeCode == B_ANY_TYPE)||(UMReadInt32(GetFieldTypePointer(iter->_currentField)) == iter->_typeCode));
}

void UMIteratorInitialize(UMessageFieldNameIterator * iter, const UMessage * msg, uint32 typeCode)
{
   iter->_message  = (UMessage *) msg;
   iter->_typeCode = typeCode;
   if (msg->_numValidBytes > MESSAGE_HEADER_SIZE)
   {
      iter->_currentField = msg->_buffer+MESSAGE_HEADER_SIZE;
      if (UMIteratorCurrentFieldMatches(iter) == false) UMIteratorAdvance(iter);
   }
   else iter->_currentField = NULL;
}

static uint32 GetNumItemsInField(const UMessage * msg, void * ftptr)
{
   const uint8 * fdata = GetFieldData(ftptr);
   uint32 numBytes = GetFieldDataLength(ftptr);
   switch(GetFieldType(ftptr))
   {
      case B_BOOL_TYPE:    return numBytes;
      case B_DOUBLE_TYPE:  return numBytes/sizeof(double);
      case B_FLOAT_TYPE:   return numBytes/sizeof(float);
      case B_INT64_TYPE:   return numBytes/sizeof(int64);
      case B_INT32_TYPE:   return numBytes/sizeof(int32);
      case B_INT16_TYPE:   return numBytes/sizeof(int16);
      case B_INT8_TYPE:    return numBytes/sizeof(int8);

      case B_MESSAGE_TYPE:
      {
         /* There's no num-items field for this type (sigh) so we gotta count them one by one */
         uint32 ret = 0;
         while(numBytes >= (sizeof(uint32)*2))
         {
            uint32 msgSize  = UMReadInt32(fdata); fdata += sizeof(uint32);
            uint32 msgMagic = UMReadInt32(fdata);
            if (msgMagic != CURRENT_PROTOCOL_VERSION)
            {
               printf("MicroMessage:  GetNumItemsInField:  sub-Message magic value at offset %i is incorrect ("UINT32_FORMAT_SPEC", should be "UINT32_FORMAT_SPEC"!)\n", (int)(fdata-msg->_buffer), msgMagic, CURRENT_PROTOCOL_VERSION);
               break;  /* paranoia -- avoid infinite loop */
            }
            if (msgSize < MESSAGE_HEADER_SIZE)
            {
               printf("MicroMessage:  GetNumItemsInField:  sub-Message size "UINT32_FORMAT_SPEC" was too small!\n", msgSize);
               break;  /* paranoia -- avoid infinite loop */
            }
            fdata += msgSize;
            uint32 moveBy = sizeof(uint32)+msgSize;
            numBytes = (numBytes>moveBy)?(numBytes-moveBy):0;
            if (fdata > (msg->_buffer+msg->_numValidBytes)) numBytes = 0;  /* paranoia */
            ret++;
         }
         return ret;
      }

      case B_POINTER_TYPE: return numBytes/sizeof(void *);
      case B_POINT_TYPE:   return numBytes/sizeof(UPoint);
      case B_RECT_TYPE:    return numBytes/sizeof(URect);
      default:             return (numBytes>=sizeof(uint32))?UMReadInt32(fdata):0;
   }
}

const char * UMIteratorGetCurrentFieldName(UMessageFieldNameIterator * iter, uint32 * optRetNumItemsInField, uint32 * optRetFieldType)
{
   if (iter->_currentField == NULL) return NULL;
   if ((optRetNumItemsInField)||(optRetFieldType))
   {
      void * ftptr = GetFieldTypePointer(iter->_currentField);
      if (optRetFieldType) *optRetFieldType = GetFieldType(ftptr);
      if (optRetNumItemsInField) *optRetNumItemsInField = GetNumItemsInField(iter->_message, ftptr);
   }
   return GetFieldName(iter->_currentField);
}

void UMIteratorAdvance(UMessageFieldNameIterator * iter)
{
   while(iter->_currentField)
   {
      void * ftptr = GetFieldTypePointer(iter->_currentField);
      uint32 fieldDataLen = GetFieldDataLength(ftptr);
      iter->_currentField = ftptr+(sizeof(uint32)+sizeof(uint32)+fieldDataLen);
      if (iter->_currentField > (iter->_message->_buffer+iter->_message->_numValidBytes))
      {
         printf("UMIteratorAdvance:  Iteration left the valid data range ("UINT32_FORMAT_SPEC" > "UINT32_FORMAT_SPEC"), aborting iteration!\n", (uint32)(iter->_currentField-iter->_message->_buffer), iter->_message->_numValidBytes);
         iter->_currentField = NULL;
      }
      else
      {
         uint32 bytesLeft = GetNumValidBytesAt(iter->_message, iter->_currentField);
         if (bytesLeft < MINIMUM_FIELD_HEADERS_SIZE)
         {
            if (bytesLeft > 0) printf("UMIteratorAdvance:  Iteration found too-short field-header ("UINT32_FORMAT_SPEC" < "UINT32_FORMAT_SPEC"), aborting iteration!\n", bytesLeft, MINIMUM_FIELD_HEADERS_SIZE);
            iter->_currentField = NULL;
         }
      }
      if (UMIteratorCurrentFieldMatches(iter)) return;
   }
}

static uint8 * GetFieldByName(const UMessage * msg, const char * fieldName, uint32 typeCode)
{
   uint32 fieldNameLen = ((uint32)strlen(fieldName))+1;

   /** First see if our cache already is pointing at the requested field.  If so, we're golden. */
   uint8 * rfc = msg->_readFieldCache;  /* lame attempt at thread-safety */
   if (rfc)
   {
      void * ftptr = GetFieldTypePointer(rfc);
      if ((GetFieldNameLength(rfc) == fieldNameLen)&&(strcmp(fieldName, GetFieldName(rfc)) == 0)) return ((typeCode == B_ANY_TYPE)||(typeCode == GetFieldType(ftptr))) ? msg->_readFieldCache : NULL;
   }

   /* If the requested field wasn't cached, find it and cache it for next time before returning it */
   rfc = GetFieldByNameAux(msg, fieldName, fieldNameLen, typeCode);
   if (rfc)
   {
      ((UMessage *)msg)->_readFieldCache = rfc;  /* for next time */
      return rfc;
   }
   else return NULL;
}

uint32 UMGetNumItemsInField(const UMessage * msg, const char * fieldName, uint32 typeCode)
{
   uint8 * field = GetFieldByName(msg, fieldName, typeCode);
   return field ? GetNumItemsInField(msg, GetFieldTypePointer(field)) : 0;
}

uint32 UMGetFieldTypeCode(const UMessage * msg, const char * fieldName)
{
   uint8 * field = GetFieldByName(msg, fieldName, B_ANY_TYPE);
   return field ? GetFieldType(GetFieldTypePointer(field)) : B_ANY_TYPE;
}

static void DoIndent(int numSpaces)
{
   int i=0;
   for (i=0; i<numSpaces; i++) putchar(' ');
}

static void PrintUMessageToStreamAux(const UMessage * msg, FILE * file, int indent);  /* forward declaration */

static void PrintUMessageFieldToStream(const UMessage * msg, const char * fieldName, FILE * file, int indent, uint32 numItems, uint32 typeCode)
{
   char pbuf[5];

   int i = 0;
   int ni = numItems;
   if (ni > 10) ni = 10;  /* truncate to avoid too much spam */

   MakePrettyTypeCodeString(typeCode, pbuf);
   DoIndent(indent); fprintf(file, "Field: Name=[%s] NumItemsInField="UINT32_FORMAT_SPEC", TypeCode=%s ("INT32_FORMAT_SPEC")\n", fieldName, numItems, pbuf, typeCode);
   for (i=0; i<ni; i++)
   {
      DoIndent(indent); fprintf(file, "  %i. ", i);
      switch(typeCode)
      {
         case B_BOOL_TYPE:    fprintf(file, "%i\n",                UMGetBool(  msg, fieldName, i)); break;
         case B_DOUBLE_TYPE:  fprintf(file, "%f\n",                UMGetDouble(msg, fieldName, i)); break;
         case B_FLOAT_TYPE:   fprintf(file, "%f\n",                UMGetFloat( msg, fieldName, i)); break;
         case B_INT64_TYPE:   fprintf(file, INT64_FORMAT_SPEC"\n", UMGetInt64( msg, fieldName, i)); break;
         case B_INT32_TYPE:   fprintf(file, INT32_FORMAT_SPEC"\n", UMGetInt32( msg, fieldName, i)); break;
         case B_INT16_TYPE:   fprintf(file, "%i\n",                UMGetInt16( msg, fieldName, i)); break;
         case B_INT8_TYPE:    fprintf(file, "%i\n",                UMGetInt8(  msg, fieldName, i)); break;

         case B_POINT_TYPE:
         {
            UPoint pt = UMGetPoint(msg, fieldName, i);
            fprintf(file, "x=%f y=%f\n", pt.x, pt.y);
         }
         break;

         case B_RECT_TYPE:
         {
            URect rc = UMGetRect(msg, fieldName, i);
            fprintf(file, "l=%f t=%f r=%f b=%f\n", rc.left, rc.top, rc.right, rc.bottom);
         }
         break;

         case B_MESSAGE_TYPE:
         {
            UMessage subMsg = UMGetMessage(msg, fieldName, i);
            if (UMGetFlattenedSize(&subMsg) > 0) PrintUMessageToStreamAux(&subMsg, file, indent+3);
                                            else fprintf(file, "(Error retrieving sub-message)\n");
         }
         break;

         case B_STRING_TYPE:
         {
            const char * s = UMGetString(msg, fieldName, i);
            if (s) fprintf(file, "[%s]\n", s);
              else fprintf(file, "(Error retrieving string value)\n");
         }
         break;

         default:
         {
            const void * subBuf;
            uint32 subBufLen;
            if (UMFindData(msg, fieldName, typeCode, i, &subBuf, &subBufLen) == B_NO_ERROR)
            {
               if (subBufLen > 0)
               {
                  const uint8 * b = (const uint8 *) subBuf;
                  int nb = subBufLen;
                  int j;

                  if (nb > 10)
                  {
                     fprintf(file, "(%i bytes, starting with", nb);
                     nb = 10;
                  }
                  else fprintf(file, "(%i bytes, equal to",nb);

                  for (j=0; j<nb; j++) fprintf(file, " %02x", b[j]);
                  if (nb < subBufLen) fprintf(file, "...)\n");
                                 else fprintf(file, ")\n");
               }
               else fprintf(file, "(zero-length buffer)\n");
            }
            else fprintf(file, "(Error retrieving binary-data value of type "UINT32_FORMAT_SPEC")\n", typeCode);
         }
         break;
      }
   }
}

static void PrintUMessageToStreamAux(const UMessage * msg, FILE * file, int indent)
{
   /* Make a pretty type code string */
   char buf[5];
   MakePrettyTypeCodeString(UMGetWhatCode(msg), buf);

   fprintf(file, "UMessage:  msg=%p, what='%s' ("INT32_FORMAT_SPEC"), fieldCount="INT32_FORMAT_SPEC", flatSize="UINT32_FORMAT_SPEC", readOnly=%i\n", msg, buf, UMGetWhatCode(msg), UMGetNumFields(msg), UMGetFlattenedSize(msg), UMIsMessageReadOnly(msg));

   indent += 2;

   {
      UMessageFieldNameIterator iter; UMIteratorInitialize(&iter, msg, B_ANY_TYPE);
      while(1)
      {
         uint32 numItems, typeCode;
         const char * fieldName = UMIteratorGetCurrentFieldName(&iter, &numItems, &typeCode);
         if (fieldName)
         {
            uint32 checkNumItems = UMGetNumItemsInField(msg, fieldName, typeCode);
            uint32 checkTypeCode = UMGetFieldTypeCode(msg, fieldName);
            if (checkNumItems != numItems) printf("ERROR, iterator said fieldName [%s] has "UINT32_FORMAT_SPEC" items, but the UMessage says it has "UINT32_FORMAT_SPEC" items!\n", fieldName, numItems, checkNumItems);
            if (checkTypeCode != typeCode) printf("ERROR, iterator said fieldName [%s] has typecode "UINT32_FORMAT_SPEC" , but the UMessage says it has typecode "UINT32_FORMAT_SPEC"\n", fieldName, typeCode, checkTypeCode);

            PrintUMessageFieldToStream(msg, fieldName, file, indent, numItems, typeCode);
            UMIteratorAdvance(&iter);
         }
         else break;
      }
   }
}

void UMPrintToStream(const UMessage * msg, FILE * optFile)
{
   PrintUMessageToStreamAux(msg, optFile?optFile:stdout, 0);
}

const UBool * UMGetBools(const UMessage * msg, const char * fieldName, uint32 * optRetNumBools)
{
   uint8 * field = GetFieldByName(msg, fieldName, B_BOOL_TYPE);
   if (field == NULL) return NULL;

   void * ftptr = GetFieldTypePointer(field);
   if (optRetNumBools) *optRetNumBools = GetNumItemsInField(msg, ftptr);
   return (const UBool *) GetFieldData(ftptr);
}

status_t UMFindBool(const UMessage * msg, const char * fieldName, uint32 idx, UBool * retBool)
{
   uint32 numBools;
   const UBool * b = UMGetBools(msg, fieldName, &numBools);
   if ((b==NULL)||(idx >= numBools)) return B_ERROR;
   *retBool = b[idx];
   return B_NO_ERROR;
}

const int8 * UMGetInt8s(const UMessage * msg, const char * fieldName, uint32 * optRetNumInt8s)
{
   uint8 * field = GetFieldByName(msg, fieldName, B_INT8_TYPE);
   if (field == NULL) return NULL;

   void * ftptr = GetFieldTypePointer(field);
   if (optRetNumInt8s) *optRetNumInt8s = GetNumItemsInField(msg, ftptr);
   return (const int8 *) GetFieldData(ftptr);
}

status_t UMFindInt8(const UMessage * msg, const char * fieldName, uint32 idx, int8 * retInt8)
{
   uint32 numInt8s;
   const int8 * b = UMGetInt8s(msg, fieldName, &numInt8s);
   if ((b==NULL)||(idx >= numInt8s)) return B_ERROR;
   *retInt8 = b[idx];
   return B_NO_ERROR;
}

const int16 * UMGetInt16s(const UMessage * msg, const char * fieldName, uint32 * optRetNumInt16s)
{
   uint8 * field = GetFieldByName(msg, fieldName, B_INT16_TYPE);
   if (field == NULL) return NULL;

   void * ftptr = GetFieldTypePointer(field);
   if (optRetNumInt16s) *optRetNumInt16s = GetNumItemsInField(msg, ftptr);
   return (const int16 *) GetFieldData(ftptr);
}

status_t UMFindInt16(const UMessage * msg, const char * fieldName, uint32 idx, int16 * retInt16)
{
   uint32 numInt16s;
   const int16 * b = UMGetInt16s(msg, fieldName, &numInt16s);
   if ((b==NULL)||(idx >= numInt16s)) return B_ERROR;
   *retInt16 = b[idx];
   return B_NO_ERROR;
}

const int32 * UMGetInt32s(const UMessage * msg, const char * fieldName, uint32 * optRetNumInt32s)
{
   uint8 * field = GetFieldByName(msg, fieldName, B_INT32_TYPE);
   if (field == NULL) return NULL;

   void * ftptr = GetFieldTypePointer(field);
   if (optRetNumInt32s) *optRetNumInt32s = GetNumItemsInField(msg, ftptr);
   return (const int32 *) GetFieldData(ftptr);
}

status_t UMFindInt32(const UMessage * msg, const char * fieldName, uint32 idx, int32 * retInt32)
{
   uint32 numInt32s;
   const int32 * b = UMGetInt32s(msg, fieldName, &numInt32s);
   if ((b==NULL)||(idx >= numInt32s)) return B_ERROR;
   *retInt32 = b[idx];
   return B_NO_ERROR;
}

const int64 * UMGetInt64s(const UMessage * msg, const char * fieldName, uint32 * optRetNumInt64s)
{
   uint8 * field = GetFieldByName(msg, fieldName, B_INT64_TYPE);
   if (field == NULL) return NULL;

   void * ftptr = GetFieldTypePointer(field);
   if (optRetNumInt64s) *optRetNumInt64s = GetNumItemsInField(msg, ftptr);
   return (const int64 *) GetFieldData(ftptr);
}

status_t UMFindInt64(const UMessage * msg, const char * fieldName, uint32 idx, int64 * retInt64)
{
   uint32 numInt64s;
   const int64 * b = UMGetInt64s(msg, fieldName, &numInt64s);
   if ((b==NULL)||(idx >= numInt64s)) return B_ERROR;
   *retInt64 = b[idx];
   return B_NO_ERROR;
}

const float * UMGetFloats(const UMessage * msg, const char * fieldName, uint32 * optRetNumFloats)
{
   uint8 * field = GetFieldByName(msg, fieldName, B_FLOAT_TYPE);
   if (field == NULL) return NULL;

   void * ftptr = GetFieldTypePointer(field);
   if (optRetNumFloats) *optRetNumFloats = GetNumItemsInField(msg, ftptr);
   return (const float *) GetFieldData(ftptr);
}

status_t UMFindFloat(const UMessage * msg, const char * fieldName, uint32 idx, float * retFloat)
{
   uint32 numFloats;
   const float * b = UMGetFloats(msg, fieldName, &numFloats);
   if ((b==NULL)||(idx >= numFloats)) return B_ERROR;
   *retFloat = b[idx];
   return B_NO_ERROR;
}

const double * UMGetDoubles(const UMessage * msg, const char * fieldName, uint32 * optRetNumDoubles)
{
   uint8 * field = GetFieldByName(msg, fieldName, B_DOUBLE_TYPE);
   if (field == NULL) return NULL;

   void * ftptr = GetFieldTypePointer(field);
   if (optRetNumDoubles) *optRetNumDoubles = GetNumItemsInField(msg, ftptr);
   return (const double *) GetFieldData(ftptr);
}

status_t UMFindDouble(const UMessage * msg, const char * fieldName, uint32 idx, double * retDouble)
{
   uint32 numDoubles;
   const double * b = UMGetDoubles(msg, fieldName, &numDoubles);
   if ((b==NULL)||(idx >= numDoubles)) return B_ERROR;
   *retDouble = b[idx];
   return B_NO_ERROR;
}

const UPoint * UMGetPoints(const UMessage * msg, const char * fieldName, uint32 * optRetNumPoints)
{
   uint8 * field = GetFieldByName(msg, fieldName, B_POINT_TYPE);
   if (field == NULL) return NULL;

   void * ftptr = GetFieldTypePointer(field);
   if (optRetNumPoints) *optRetNumPoints = GetNumItemsInField(msg, ftptr);
   return (const UPoint *) GetFieldData(ftptr);
}

status_t UMFindPoint(const UMessage * msg, const char * fieldName, uint32 idx, UPoint * retPoint)
{
   uint32 numPoints;
   const UPoint * b = UMGetPoints(msg, fieldName, &numPoints);
   if ((b==NULL)||(idx >= numPoints)) return B_ERROR;
   *retPoint = b[idx];
   return B_NO_ERROR;
}

const URect * UMGetRects(const UMessage * msg, const char * fieldName, uint32 * optRetNumRects)
{
   uint8 * field = GetFieldByName(msg, fieldName, B_RECT_TYPE);
   if (field == NULL) return NULL;

   void * ftptr = GetFieldTypePointer(field);
   if (optRetNumRects) *optRetNumRects = GetNumItemsInField(msg, ftptr);
   return (const URect *) GetFieldData(ftptr);
}

status_t UMFindRect(const UMessage * msg, const char * fieldName, uint32 idx, URect * retRect)
{
   uint32 numRects;
   const URect * b = UMGetRects(msg, fieldName, &numRects);
   if ((b==NULL)||(idx >= numRects)) return B_ERROR;
   *retRect = b[idx];
   return B_NO_ERROR;
}

const char * UMGetString(const UMessage * msg, const char * fieldName, uint32 idx)
{
   uint8 * field = GetFieldByName(msg, fieldName, B_STRING_TYPE);
   if (field == NULL) return NULL;

   void * ftptr = GetFieldTypePointer(field);
   if (idx >= GetNumItemsInField(msg, ftptr)) return NULL;

   const uint8 * afterEndOfField = GetFieldData(ftptr)+GetFieldDataLength(ftptr);
   const uint8 * pointerToString = ((uint8 *)ftptr)+(4*sizeof(uint32));  /* skip past the field-type, field-size, number-of-items, and first-string-length fields */
   while(idx > 0)
   {
      uint32 stringSize = UMReadInt32(pointerToString-sizeof(uint32));
      if ((stringSize+sizeof(uint32)) > (afterEndOfField-pointerToString)) return NULL;  /* paranoia */
      pointerToString += UMReadInt32(pointerToString-sizeof(uint32))+sizeof(uint32);  /* move past the string and the next string's string-length-field */
      idx--;
   }
   return (const char *) pointerToString;
}

status_t UMFindData(const UMessage * msg, const char * fieldName, uint32 dataType, uint32 idx, const void ** retDataBytes, uint32 * retNumBytes)
{
   uint8 * field = GetFieldByName(msg, fieldName, dataType);
   if (field == NULL) return B_ERROR;

   void * ftptr = GetFieldTypePointer(field);
   if (idx >= GetNumItemsInField(msg, ftptr)) return B_ERROR;

   const uint8 * afterEndOfField = GetFieldData(ftptr)+GetFieldDataLength(ftptr);
   const uint8 * pointerToBlob = ((uint8 *)ftptr)+(4*sizeof(uint32));  /* skip past the field-type, field-size, num-items, and first-blob-length fields */
   while(idx > 0)
   {
      uint32 blobSize = UMReadInt32(pointerToBlob-sizeof(uint32));  /* move past the blob and the next blob's string-length-field */
      if ((blobSize+sizeof(uint32)) > (afterEndOfField-pointerToBlob)) return B_ERROR;  // paranoia
      pointerToBlob += blobSize+sizeof(uint32);  /* move past the blob and the next blob's string-length-field */
      idx--;
   }
   *retDataBytes = pointerToBlob;
   *retNumBytes  = UMReadInt32(pointerToBlob-sizeof(uint32));
   return B_NO_ERROR;
}

status_t UMFindMessage(const UMessage * msg, const char * fieldName, uint32 idx, UMessage * retMessage)
{
   uint8 * field = GetFieldByName(msg, fieldName, B_MESSAGE_TYPE);
   if (field == NULL) return B_ERROR;

   void * ftptr = GetFieldTypePointer(field);
   const uint8 * afterEndOfField = GetFieldData(ftptr)+GetFieldDataLength(ftptr);
   const uint8 * pointerToMsg = ((uint8 *)ftptr)+(3*sizeof(uint32));  /* skip past the field-type, field-size, and first-msg-length fields (there is no field-size field) */
   while(idx > 0)
   {
      uint32 msgSize = UMReadInt32(pointerToMsg-sizeof(uint32));
      if ((msgSize < MESSAGE_HEADER_SIZE)||((msgSize+sizeof(uint32)) > (afterEndOfField-pointerToMsg))) return B_ERROR;  /* paranoia */
      pointerToMsg += msgSize+sizeof(uint32);  /* move past the msg and the next msg's msg-length-field */
      idx--;
   }
   return UMInitializeWithExistingData(retMessage, pointerToMsg, UMReadInt32(pointerToMsg-sizeof(uint32)));
}

#ifdef cplusplus
};
#endif
