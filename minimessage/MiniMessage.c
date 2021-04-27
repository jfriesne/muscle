#include <string.h>
#include "minimessage/MiniMessage.h"

#ifdef cplusplus
extern "C" {
#else
// Generate a compile-time error if MuscleSupport.h didn't get the typedefs right
char   int8_is_1_byte_assertion [(sizeof( int8)  == 1) ? 1 : -1];
char  uint8_is_1_byte_assertion [(sizeof(uint8)  == 1) ? 1 : -1];
char  int16_is_2_bytes_assertion[(sizeof( int16) == 2) ? 1 : -1];
char uint16_is_2_bytes_assertion[(sizeof(uint16) == 2) ? 1 : -1];
char  int32_is_4_bytes_assertion[(sizeof( int32) == 4) ? 1 : -1];
char uint32_is_4_bytes_assertion[(sizeof(uint32) == 4) ? 1 : -1];
char  float_is_4_bytes_assertion[(sizeof(float)  == 4) ? 1 : -1];
char  float_is_4_bytes_assertion[(sizeof(float)  == 4) ? 1 : -1];
char  int64_is_8_bytes_assertion[(sizeof( int64) == 8) ? 1 : -1];
char uint64_is_8_bytes_assertion[(sizeof(uint64) == 8) ? 1 : -1];
char double_is_8_bytes_assertion[(sizeof(double) == 8) ? 1 : -1];
char double_is_8_bytes_assertion[(sizeof(double) == 8) ? 1 : -1];
#endif

static uint32 _allocedBytes = 0;  /* for memory-leak debugging */

uint32 MGetNumBytesAllocated() {return _allocedBytes;}

#ifdef MUSCLE_ENABLE_MEMORY_TRACKING

void * MMalloc(uint32 numBytes)
{
   char * ret = (char *) malloc(sizeof(uint32)+numBytes);
   if (ret)
   {
      memcpy(ret, &numBytes, sizeof(numBytes));
      _allocedBytes += numBytes;
/*printf("++" UINT32_FORMAT_SPEC " -> " UINT32_FORMAT_SPEC "\n", numBytes, _allocedBytes);*/
      return ret+sizeof(uint32);
   }
   return NULL;
}

void MFree(void * ptr)
{
   if (ptr)
   {
      char * rawPtr = ((char *)ptr)-sizeof(uint32);
      uint32 allocSize; memcpy(&allocSize, rawPtr, sizeof(allocSize));
      _allocedBytes -= allocSize;
/*printf("--" UINT32_FORMAT_SPEC " -> " UINT32_FORMAT_SPEC "\n", allocSize, _allocedBytes);*/
      free(rawPtr);
   }
}

void * MRealloc(void * oldBuf, uint32 newSize)
{
   uint32 oldSize;
   if (oldBuf) memcpy(&oldSize, ((char *)oldBuf)-sizeof(uint32), sizeof(oldSize));
          else oldSize = 0;

   if (newSize == oldSize) return oldBuf;
   else
   {
      void * newBuf = (newSize > 0) ? MMalloc(newSize) : NULL;
      if ((newSize > 0)&&(newBuf == NULL)) return NULL;  // out-of-memory error!  Avoid side effects

      if ((newBuf)&&(oldBuf)) memcpy(newBuf, oldBuf, (newSize<oldSize)?newSize:oldSize);
      if (oldBuf) MFree(oldBuf);
      return newBuf;
   }
}

#endif

static c_status_t ReadData(const uint8 * inBuf, uint32 inputBufferBytes, uint32 * readOffset, void * copyTo, uint32 blockSize)
{
   if ((*readOffset + blockSize) > inputBufferBytes) return CB_ERROR;
   memcpy(copyTo, &inBuf[*readOffset], blockSize);
   *readOffset += blockSize;
   return CB_NO_ERROR;
};     

static void WriteData(uint8 * outBuf, uint32 * writeOffset, const void * copyFrom, uint32 blockSize)
{
   memcpy(&outBuf[*writeOffset], copyFrom, blockSize);
   *writeOffset += blockSize;
};     

#define OLDEST_SUPPORTED_PROTOCOL_VERSION 1347235888 /* 'PM00' */
#define CURRENT_PROTOCOL_VERSION          1347235888 /* 'PM00' */

struct _MMessageField;  /* forward declaration */
struct _MMessageField {
   struct _MMessageField * prevField;  /* used by our linked list      */
   struct _MMessageField * nextField;  /* used by our linked list      */
   uint32 allocSize;  /* Our allocation size, for convenience          */
   const char * name; /* pointer to our field name, for convenience    */
   uint32 nameBytes;  /* number of bytes in field name (including NUL) */
   uint32 typeCode;
   void * data;       /* Pointer to our data bytes, for convenience    */
   uint32 itemSize;   /* item size, for convenience                    */
   uint32 numItems;   /* number of item-slots in this field            */
   MBool isFixedSize;  /* If MFalse, data is an array of MByteBuffers  */
   MBool isFlattenable; /* If MFalse, data shouldn't be Flattened      */
   /* ... data bytes here ...                                          */
   /* name bytes start here, after the last data byte                  */
   /* Note that name bytes must be last, or MMRenameField() breaks     */
};
typedef struct _MMessageField MMessageField;

/* Our internal implementation of the MMessage object... known only to the   */
/* code inside this file!  All external code shouldn't need to know about this. */
struct _MMessage {
   uint32 what;
   uint32 numFields;
   MMessageField * firstField;
   MMessageField * lastField;

   struct _MMessage * scratch;  /* scratch variable, used in Unflatten */
};

MByteBuffer * MBAllocByteBuffer(uint32 numBytes, MBool clearBytes)
{
   MByteBuffer * mbb = (MByteBuffer *) MMalloc((sizeof(MByteBuffer)+numBytes)-1);  /* -1 since at least 1 data byte is in the struct */
   if (mbb)
   {
      mbb->numBytes = numBytes;
      if (clearBytes) memset(&mbb->bytes, 0, numBytes);
   }
   return mbb;
}

MByteBuffer * MBStrdupByteBuffer(const char * sourceString)
{
   const int slen = ((int)strlen(sourceString))+1;
   MByteBuffer * mbb = MBAllocByteBuffer(slen, MFalse);
   if (mbb) memcpy(&mbb->bytes, sourceString, slen);
   return mbb;
}

MByteBuffer * MBCloneByteBuffer(const MByteBuffer * cloneMe)
{
   MByteBuffer * mbb = MBAllocByteBuffer(cloneMe->numBytes, MFalse);
   if (mbb) memcpy(&mbb->bytes, &cloneMe->bytes, mbb->numBytes);
   return mbb;
}

MBool MBAreByteBuffersEqual(const MByteBuffer * buf1, const MByteBuffer * buf2)
{
   return ((buf1 == buf2)||((buf1->numBytes == buf2->numBytes)&&(memcmp(&buf1->bytes, &buf2->bytes, buf1->numBytes) == 0)));
}

void MBFreeByteBuffer(MByteBuffer * msg)
{
   /* yup, it's that easy... but you should still always call this function        */
   /* instead of calling MFree() directly, in case the implementation changes later */
   if (msg) MFree(msg);
}

/* Paranoia:  when allocating a MMessageField struct, we often allocate storage for
 * its corresponding data as part of the same allocation, and we need to make sure
 * that the data is longword-aligned to avoid data-alignment issues (particularly 
 * under ARM), so this macro rounds-up the passed-in-value to the nearest multiple of 8.
 */
#define WITH_ALIGNMENT_PADDING(rawSize) (sizeof(uint64)*(((((rawSize)+sizeof(uint64))-1)/sizeof(uint64))))

/* Note that numNameBytes includes the NUL byte! */
static MMessageField * AllocMMessageField(const char * fieldName, uint32 numNameBytes, uint32 typeCode, uint32 numItems, uint32 itemSize)
{
   if (numItems > 0)
   {
      const uint32 dataSize  = numItems*itemSize;
      const uint32 allocSize = WITH_ALIGNMENT_PADDING(sizeof(MMessageField))+dataSize+numNameBytes;
      MMessageField * ret = (MMessageField *) MMalloc(allocSize);
      if (ret)
      {
         ret->prevField   = ret->nextField = NULL;
         ret->allocSize   = allocSize;
         ret->nameBytes   = numNameBytes;
         ret->typeCode    = typeCode;
         ret->itemSize    = itemSize;
         ret->numItems    = numItems;
         ret->data        = ((char *)ret) + WITH_ALIGNMENT_PADDING(sizeof(MMessageField));  /* data bytes start immediately after the struct */
         ret->name        = ((char *)ret->data)+dataSize;  /* name starts right after the data */
         ret->isFixedSize = ret->isFlattenable = MTrue;
         memset(ret->data, 0, dataSize);
         memcpy((void *)ret->name, fieldName, numNameBytes);
         ((char *)ret->name)[ret->nameBytes-1] = '\0';  /* paranoia:  guarantee that the field name is terminated */
      }
      return ret;
   }
   else return NULL;  /* we don't allow zero-item fields! */
}

static MMessageField * CloneMMessageField(const MMessageField * cloneMe)
{
   MMessageField * clone = (MMessageField *) MMalloc(cloneMe->allocSize);
   if (clone)
   {
      memcpy(clone, cloneMe, cloneMe->isFixedSize ? cloneMe->allocSize : sizeof(MMessageField));
      clone->prevField = clone->nextField = NULL;
      clone->data      = ((char *)clone) + (((char *)cloneMe->data)-((char *)cloneMe));
      clone->name      = ((const char *)clone) + (cloneMe->name-((const char *)cloneMe));
      if (clone->isFixedSize == MFalse)
      {
         /* Oops, this field has alloced-pointer semantics, so we have to clone all the items too */
         if (clone->typeCode == B_MESSAGE_TYPE)
         {
            MMessage ** dstArray = (MMessage **) clone->data;
            const MMessage ** srcArray = (const MMessage **) cloneMe->data;
            int32 i;
            for (i=cloneMe->numItems-1; i>=0; i--)
            {
               if (srcArray[i])
               {
                  if ((dstArray[i] = MMCloneMessage(srcArray[i])) == NULL)
                  {
                     /* Allocation failure!  Roll back previous allocs and fail cleanly */
                     int32 j;
                     for (j=cloneMe->numItems-1; j>i; j--) MMFreeMessage(dstArray[j]);
                     MFree(clone);
                     return NULL;
                  }
               }
               else dstArray[i] = NULL;
            }
         }
         else
         {
            MByteBuffer ** dstArray = (MByteBuffer **) clone->data;
            const MByteBuffer ** srcArray = (const MByteBuffer **) cloneMe->data;
            int32 i;
            for (i=cloneMe->numItems-1; i>=0; i--)
            {
               if (srcArray[i])
               {
                  if ((dstArray[i] = MBCloneByteBuffer(srcArray[i])) == NULL)
                  {
                     /* Allocation failure!  Roll back previous allocs and fail cleanly */
                     int32 j;
                     for (j=cloneMe->numItems-1; j>i; j--) MBFreeByteBuffer(dstArray[j]);
                     MFree(clone);
                     return NULL;
                  }
               }
               else dstArray[i] = NULL;
            }
         }

         /* copy the name too, since we didn't do it above */
         memcpy((char *)clone->name, cloneMe->name, clone->nameBytes);
      }
   }
   return clone;
}

static void FreeMMessageField(MMessageField * field)
{
   if (field)
   {
      if (field->isFixedSize == MFalse)
      {
         /* Oops, this field has alloced-pointer semantics, so we have to free all the items too */
         if (field->typeCode == B_MESSAGE_TYPE)
         {
            MMessage ** array = (MMessage **) field->data;
            int32 i;
            for (i=field->numItems-1; i>=0; i--) MMFreeMessage(array[i]);
         }
         else
         {
            MByteBuffer ** array = (MByteBuffer **) field->data;
            int32 i;
            for (i=field->numItems-1; i>=0; i--) MBFreeByteBuffer(array[i]);
         }
      }
      MFree(field);
   }
}

MMessage * MMAllocMessage(uint32 what)
{
   MMessage * ret = (MMessage *) MMalloc(sizeof(MMessage));   
   if (ret)
   {
      ret->what       = what;
      ret->numFields  = 0;
      ret->firstField = ret->lastField = NULL;
   }
   return ret;
}

MMessage * MMCloneMessage(const MMessage * cloneMe)
{
   MMessage * clone = MMAllocMessage(cloneMe->what);   
   if (clone)
   {
      const MMessageField * nextField = cloneMe->firstField;
      while(nextField)
      {
         MMessageField * cloneField = CloneMMessageField(nextField);
         if (cloneField)
         {
            if (clone->lastField)
            {
               clone->lastField->nextField = cloneField;
               cloneField->prevField       = clone->lastField;
               clone->lastField            = cloneField;
            }
            else clone->firstField = clone->lastField = cloneField;

            clone->numFields++;
         }
         else
         {
            MMFreeMessage(clone);
            return NULL;
         }
         nextField = nextField->nextField;
      }
   }
   return clone;
}

void MMFreeMessage(MMessage * msg)
{
   if (msg)
   {
      MMClearMessage(msg);
      MFree(msg);
   }
}

uint32 MMGetWhat(const MMessage * msg)
{
   return msg->what;
}

void MMSetWhat(MMessage * msg, uint32 newWhat) 
{
   msg->what = newWhat;
}

void MMClearMessage(MMessage * msg)
{
   MMessageField * field = msg->firstField;
   while(field)
   {
      MMessageField * nextField = field->nextField;
      FreeMMessageField(field);
      field = nextField;
   }
   msg->firstField = msg->lastField = NULL;
   msg->numFields = 0;
}

/** This function adds the given field into msg's linked list of fields. */
static void AddMMessageField(MMessage * msg, MMessageField * field)
{
   field->prevField = msg->lastField;
   if (msg->lastField) msg->lastField->nextField = field;
                  else msg->firstField = field; 
   msg->lastField = field;
   msg->numFields++;
}

static MBool IsTypeCodeVariableSize(uint32 typeCode)
{
   /* Don't allow the user to put data blobs under any of the non-data-blob type codes, */
   /* because that would mess up our internal logic which assumes that fields of those  */
   /* types have the appropriate data for their type                                    */
   switch(typeCode)
   {
      case B_BOOL_TYPE:    case B_DOUBLE_TYPE: case B_FLOAT_TYPE: case B_INT64_TYPE:
      case B_INT32_TYPE:   case B_INT16_TYPE:  case B_INT8_TYPE:
      case B_POINTER_TYPE: case B_POINT_TYPE:  case B_RECT_TYPE:
         return MFalse;

      default:
         return MTrue;
   }
}

static MMessageField * LookupMMessageField(const MMessage * msg, const char * fieldName, uint32 typeCode)
{
   MMessageField * f = msg->lastField;  /* assuming we're more likely to look up what we added most recently (?) */
   while(f)
   {
      if (strcmp(f->name, fieldName) == 0) return ((typeCode == B_ANY_TYPE)||(typeCode == f->typeCode)) ? f : NULL;
      f = f->prevField;
   }
   return NULL;  /* field not found */
}

static void DetachMMessageField(MMessage * msg, MMessageField * field)
{
   if (field->prevField) field->prevField->nextField = field->nextField;
   if (field->nextField) field->nextField->prevField = field->prevField;
   if (field == msg->lastField)  msg->lastField  = field->prevField;
   if (field == msg->firstField) msg->firstField = field->nextField;
   field->prevField = field->nextField = NULL;
   msg->numFields--;
}

/** This function handles placing a new variable-sized-data-blob field into a Message */
static MByteBuffer ** PutMMVariableFieldAux(MMessage * msg, MBool retainOldData, uint32 typeCode, const char * fieldName, uint32 numItems)
{
   MMessageField * newField = AllocMMessageField(fieldName, ((uint32)(strlen(fieldName)))+1, typeCode, numItems, sizeof(MByteBuffer *));
   if (newField)
   {
      MMessageField * oldField = LookupMMessageField(msg, fieldName, B_ANY_TYPE);
      newField->isFixedSize = MFalse;
      if (oldField)
      {
         if ((retainOldData)&&(oldField->typeCode == typeCode))
         {
            const uint32 commonItems = (numItems < oldField->numItems) ? numItems : oldField->numItems;
            uint32 i;
            MByteBuffer ** fromArray = (MByteBuffer **) oldField->data;
            MByteBuffer ** toArray   = (MByteBuffer **) newField->data;
            for (i=0; i<commonItems; i++) 
            {
               toArray[i]   = fromArray[i];
               fromArray[i] = NULL;
            }
         }
         DetachMMessageField(msg, oldField);
         FreeMMessageField(oldField);
      }
      AddMMessageField(msg, newField);
   }
   return newField ? (MByteBuffer **)newField->data : NULL;
}

static void * GetFieldDataAux(const MMessage * msg, const char * fieldName, uint32 typeCode, uint32 * optRetNumItems)
{
   MMessageField * f = LookupMMessageField(msg, fieldName, typeCode); 
   if ((f)&&(f->numItems > 0))
   {
      if (optRetNumItems) *optRetNumItems = f->numItems;
      return f->data;
   }
   return NULL;
}

static void * PutMMFixedFieldAux(MMessage * msg, MBool retainOldData, uint32 typeCode, const char * fieldName, uint32 numItems, uint32 itemSize)
{
   MMessageField * newField = AllocMMessageField(fieldName, ((uint32)strlen(fieldName))+1, typeCode, numItems, itemSize);
   if (newField)
   {
      MMessageField * oldField = LookupMMessageField(msg, fieldName, B_ANY_TYPE);
      if (oldField)
      {
         if ((retainOldData)&&(oldField->typeCode == typeCode)) memcpy(newField->data, oldField->data, ((numItems < oldField->numItems) ? numItems : oldField->numItems)*itemSize);
         DetachMMessageField(msg, oldField);
         FreeMMessageField(oldField);
      }
      AddMMessageField(msg, newField);
   }
   return newField ? newField->data : NULL;
}

c_status_t MMRemoveField(MMessage * msg, const char * fieldName)
{
   MMessageField * field = LookupMMessageField(msg, fieldName, B_ANY_TYPE);
   if (field)
   {
      DetachMMessageField(msg, field);
      FreeMMessageField(field);
      return CB_NO_ERROR;
   }
   return CB_ERROR;
}

MByteBuffer ** MMPutStringField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems)
{
   return PutMMVariableFieldAux(msg, retainOldData, B_STRING_TYPE, fieldName, numItems);
}

MBool * MMPutBoolField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems)
{
   return (MBool *) PutMMFixedFieldAux(msg, retainOldData, B_BOOL_TYPE, fieldName, numItems, sizeof(MBool));
}

int8 * MMPutInt8Field(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems)
{
   return (int8 *) PutMMFixedFieldAux(msg, retainOldData, B_INT8_TYPE, fieldName, numItems, sizeof(int8));
}

int16 * MMPutInt16Field(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems)
{
   return (int16 *) PutMMFixedFieldAux(msg, retainOldData, B_INT16_TYPE, fieldName, numItems, sizeof(int16));
}

int32 * MMPutInt32Field(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems)
{
   return (int32 *) PutMMFixedFieldAux(msg, retainOldData, B_INT32_TYPE, fieldName, numItems, sizeof(int32));
}

int64 * MMPutInt64Field(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems)
{
   return (int64 *) PutMMFixedFieldAux(msg, retainOldData, B_INT64_TYPE, fieldName, numItems, sizeof(int64));
}

float * MMPutFloatField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems)
{
   return (float *) PutMMFixedFieldAux(msg, retainOldData, B_FLOAT_TYPE, fieldName, numItems, sizeof(float));
}

double * MMPutDoubleField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems)
{
   return (double *) PutMMFixedFieldAux(msg, retainOldData, B_DOUBLE_TYPE, fieldName, numItems, sizeof(double));
}

MMessage ** MMPutMessageField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems)
{
   MMessageField * newField = AllocMMessageField(fieldName, ((uint32)strlen(fieldName))+1, B_MESSAGE_TYPE, numItems, sizeof(MMessage *));
   if (newField)
   {
      MMessageField * oldField = LookupMMessageField(msg, fieldName, B_ANY_TYPE);
      newField->isFixedSize = MFalse;
      if (oldField)
      {
         if ((retainOldData)&&(oldField->typeCode == B_MESSAGE_TYPE))
         {
            const uint32 commonItems = (numItems < oldField->numItems) ? numItems : oldField->numItems;
            uint32 i;
            MMessage ** fromArray = (MMessage **) oldField->data;
            MMessage ** toArray   = (MMessage **) newField->data;
            for (i=0; i<commonItems; i++) 
            {
               toArray[i]   = fromArray[i];
               fromArray[i] = NULL;
            }
         }
         DetachMMessageField(msg, oldField);
         FreeMMessageField(oldField);
      }
      AddMMessageField(msg, newField);
   }
   return newField ? (MMessage **)newField->data : NULL;
}

void ** MMPutPointerField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems)
{
   void ** ret = (void **) PutMMFixedFieldAux(msg, retainOldData, B_POINTER_TYPE, fieldName, numItems, sizeof(void *));
   if (ret)
   {
      MMessageField * f = LookupMMessageField(msg, fieldName, B_POINTER_TYPE);
      if (f) f->isFlattenable = MFalse;
   }
   return ret;
}

MPoint * MMPutPointField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems)
{
   return (MPoint *) PutMMFixedFieldAux(msg, retainOldData, B_POINT_TYPE, fieldName, numItems, sizeof(MPoint));
}

MRect * MMPutRectField(MMessage * msg, MBool retainOldData, const char * fieldName, uint32 numItems)
{
   return (MRect *) PutMMFixedFieldAux(msg, retainOldData, B_RECT_TYPE, fieldName, numItems, sizeof(MRect));
}

MByteBuffer ** MMPutDataField(MMessage * msg, MBool retainOldData, uint32 typeCode, const char * fieldName, uint32 numItems)
{
   return ((typeCode != B_MESSAGE_TYPE)&&(IsTypeCodeVariableSize(typeCode))) ? PutMMVariableFieldAux(msg, retainOldData, typeCode, fieldName, numItems) : NULL;
}

static uint32 GetMMessageFieldFlattenedSize(const MMessageField * f, MBool includeHeaders)
{
   uint32 sum = includeHeaders ? (sizeof(uint32) + f->nameBytes + sizeof(uint32) + sizeof(uint32)) : 0; /* Entry name length, entry name, string, type code, entry data length */
   if (IsTypeCodeVariableSize(f->typeCode))
   {
      uint32 numItems = f->numItems;
      uint32 i;

      sum += (numItems*sizeof(uint32));  /* because each sub-item is preceded by its byte-count */
      if (f->typeCode == B_MESSAGE_TYPE)
      {
         /* Note that NULL entries will be flattened as empty Messages, since the protocol doesn't support them directly. */
         const MMessage ** msgs = (const MMessage **) f->data;
         for (i=0; i<numItems; i++) sum += msgs[i] ? MMGetFlattenedSize(msgs[i]) : (3*sizeof(uint32));
      }
      else
      {
         const MByteBuffer ** bufs = (const MByteBuffer **) f->data;
         sum += sizeof(uint32);  /* num-items-in-array field */
         for (i=0; i<numItems; i++) sum += bufs[i] ? bufs[i]->numBytes : 0;
      }
   }
   else sum += f->numItems*f->itemSize;  /* Entry data length and data items */

   return sum;
}

/* Copies (numItems) items from (in) to (out), swapping the byte gender of each item as it goes. */
static void SwapCopy(void * out, const void * in, uint32 numItems, uint32 itemSize)
{
   if (itemSize > 1)
   {
      const int numBytes = numItems * itemSize;
      int whichByte;
      for (whichByte=itemSize-1; whichByte>=0; whichByte--)
      {
         int i;
         uint8 * out8      = ((uint8 *)out)+whichByte;
         const uint8 * in8 = ((const uint8 *)in)+itemSize-(whichByte+1);
         for(i=0; i<numBytes; i+=itemSize) out8[i] = in8[i];
      }
   }
   else memcpy(out, in, numItems);
}

static uint32 FlattenMMessageField(const MMessageField * f, void * outBuf)
{
   /* 3. Entry name length (4 bytes)          */
   /* 4. Entry name string (flattened String) */
   /* 5. Entry type code (4 bytes)            */
   /* 6. Entry data length (4 bytes)          */
   /* 7. Entry data (n bytes)                 */

   /* Write entry name length */
   uint32 writeOffset = 0;
   uint8 * buffer = (uint8 *) outBuf;
   {
      const uint32 networkByteOrder = B_HOST_TO_LENDIAN_INT32(f->nameBytes);
      WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));
   }

   /* Write entry name */
   memcpy(&buffer[writeOffset], f->name, f->nameBytes); writeOffset += f->nameBytes;

   /* Write entry type code */
   {
      const uint32 networkByteOrder = B_HOST_TO_LENDIAN_INT32(f->typeCode);
      WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));
   }

   /* Write entry data length */
   {
      const uint32 networkByteOrder = B_HOST_TO_LENDIAN_INT32(GetMMessageFieldFlattenedSize(f, MFalse));
      WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));
   }

   /* Write entry data */
   if (IsTypeCodeVariableSize(f->typeCode))
   {
      const uint32 numItems = f->numItems;

      if (f->typeCode == B_MESSAGE_TYPE)
      {
         /* Note that NULL entries will be flattened as empty Messages, since the protocol doesn't support them directly. */
         /* Note also that for this type the number-of-items-in-array field is NOT written into the buffer (sigh)         */
         const MMessage ** msgs = (const MMessage **) f->data;
         uint32 i;
         for (i=0; i<numItems; i++) 
         {
            const MMessage * subMsg = msgs[i];
            const uint32 msgSize    = subMsg ? MMGetFlattenedSize(subMsg) : (3*sizeof(uint32));
            uint32 networkByteOrder = B_HOST_TO_LENDIAN_INT32(msgSize);
            WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));
            if (subMsg) 
            {
               MMFlattenMessage(subMsg, &buffer[writeOffset]); writeOffset += msgSize;
            }
            else
            {
               /* No MMessage present?  Then we're going to have to fake one                          */
               /* Format:  0. Protocol revision number (4 bytes, always set to CURRENT_PROTOCOL_VERSION) */
               /*          1. 'what' code (4 bytes)                                                      */
               /*          2. Number of entries (4 bytes)                                                */

               /* Write current protocol version */
               networkByteOrder = B_HOST_TO_LENDIAN_INT32(CURRENT_PROTOCOL_VERSION);
               WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));

               /* Write 'what' code (zero) */
               networkByteOrder = B_HOST_TO_LENDIAN_INT32(0);
               WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));

               /* Write number of entries (zero) */
               networkByteOrder = B_HOST_TO_LENDIAN_INT32(0);
               WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));
            }
         }
      }
      else
      {
         const MByteBuffer ** bufs = (const MByteBuffer **) f->data;

         /* Write the number of items in the array */
         {
            const uint32 networkByteOrder = B_HOST_TO_LENDIAN_INT32(numItems);
            WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));
         }

         uint32 i;
         for (i=0; i<numItems; i++) 
         {
            const uint32 bufSize = bufs[i] ? bufs[i]->numBytes : 0;
            const uint32 networkByteOrder = B_HOST_TO_LENDIAN_INT32(bufSize);
            WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));
            memcpy(&buffer[writeOffset], &bufs[i]->bytes, bufSize); writeOffset += bufSize;
         }
      }
   }
   else
   {
      const uint32 dataSize = f->numItems*f->itemSize;
      MBool doMemcpy = MTrue;
      if (BYTE_ORDER != LITTLE_ENDIAN)
      {
         doMemcpy = MFalse;
         switch(f->typeCode)
         {
            case B_BOOL_TYPE: case B_INT8_TYPE: doMemcpy = MTrue;                                      break;
            case B_POINT_TYPE:  SwapCopy(&buffer[writeOffset], f->data, f->numItems*2, sizeof(float)); break;
            case B_RECT_TYPE:   SwapCopy(&buffer[writeOffset], f->data, f->numItems*4, sizeof(float)); break;
            default:            SwapCopy(&buffer[writeOffset], f->data, f->numItems, f->itemSize);     break;
         }
      }
      if (doMemcpy) memcpy(&buffer[writeOffset], f->data, dataSize);
      writeOffset += dataSize;
   }

   return writeOffset;
}

uint32 MMGetFlattenedSize(const MMessage * msg)
{  
   /* For the message header:  4 bytes for the protocol revision #, 4 bytes for the number-of-entries field, 4 bytes for what code */
   uint32 sum = 3 * sizeof(uint32); 

   const MMessageField * f = msg->firstField;
   while(f)
   {
      if (f->isFlattenable) sum += GetMMessageFieldFlattenedSize(f, MTrue);
      f = f->nextField;
   }
   return sum;       
}

void MMFlattenMessage(const MMessage * msg, void * outBuf)
{
   /* Format:  0. Protocol revision number (4 bytes, always set to CURRENT_PROTOCOL_VERSION) */
   /*          1. 'what' code (4 bytes)                                                      */
   /*          2. Number of entries (4 bytes)                                                */
   /*          3. Entry name length (4 bytes)                                                */
   /*          4. Entry name string (flattened String)                                       */
   /*          5. Entry type code (4 bytes)                                                  */
   /*          6. Entry data length (4 bytes)                                                */
   /*          7. Entry data (n bytes)                                                       */
   /*          8. loop to 3 as necessary                                                     */

   /* Write current protocol version */
   uint32 writeOffset = 0;
   {
      const uint32 networkByteOrder = B_HOST_TO_LENDIAN_INT32(CURRENT_PROTOCOL_VERSION);
      WriteData(outBuf, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));
   }

   /* Write 'what' code */
   {
      const uint32 networkByteOrder = B_HOST_TO_LENDIAN_INT32(msg->what);
      WriteData(outBuf, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));
   }

   /* Calculate the number of flattenable entries (may be less than the total number of entries!) */
   {
      uint32 numFlattenableEntries = 0;
      {
         const MMessageField * f = msg->firstField;
         while(f)
         {
            if (f->isFlattenable) numFlattenableEntries++;
            f = f->nextField;
         }
      }

      /* Write number of entries */
      {
         const uint32 networkByteOrder = B_HOST_TO_LENDIAN_INT32(numFlattenableEntries);
         WriteData(outBuf, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));
      }

      /* Write entries */
      {
         const MMessageField * f = msg->firstField;
         while(f)
         {
            if (f->isFlattenable) writeOffset += FlattenMMessageField(f, &((char *)outBuf)[writeOffset]);
            f = f->nextField;
         }
      }
   }
/*printf("%s " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", (writeOffset != MMGetFlattenedSize(msg))?"ERROR":"ok", writeOffset, MMGetFlattenedSize(msg)); DEBUG */
}

static MMessageField * ImportMMessageField(const char * fieldName, uint32 nameLength, uint32 tc, uint32 eLength, const void * dataPtr, uint32 itemSize, uint32 swapSize)
{
   const uint32 numItems = eLength/itemSize;
   MMessageField * field = AllocMMessageField(fieldName, nameLength, tc, numItems, itemSize);
   if (field)
   {
      if (BYTE_ORDER != LITTLE_ENDIAN) SwapCopy(field->data, dataPtr, numItems*(itemSize/swapSize), swapSize);
                                  else memcpy(field->data,   dataPtr, numItems*itemSize);
      return field;
   }
   return NULL;
}

c_status_t MMUnflattenMessage(MMessage * msg, const void * inBuf, uint32 inputBufferBytes)
{
   uint32 readOffset = 0;
   const uint8 * buffer = (const uint8 *) inBuf;

   /* Read and check protocol version number */
   uint32 networkByteOrder, numEntries;
   {
      uint32 messageProtocolVersion;
      if (ReadData(buffer, inputBufferBytes, &readOffset, &networkByteOrder, sizeof(networkByteOrder)) != CB_NO_ERROR) return CB_ERROR;

      messageProtocolVersion = B_LENDIAN_TO_HOST_INT32(networkByteOrder);
      if ((messageProtocolVersion < OLDEST_SUPPORTED_PROTOCOL_VERSION)||(messageProtocolVersion > CURRENT_PROTOCOL_VERSION)) return CB_ERROR;
   
      /* Read 'what' code */
      if (ReadData(buffer, inputBufferBytes, &readOffset, &networkByteOrder, sizeof(networkByteOrder)) != CB_NO_ERROR) return CB_ERROR;
      msg->what = B_LENDIAN_TO_HOST_INT32(networkByteOrder);

      /* Read number of entries */
      if (ReadData(buffer, inputBufferBytes, &readOffset, &networkByteOrder, sizeof(networkByteOrder)) != CB_NO_ERROR) return CB_ERROR;
      numEntries = B_LENDIAN_TO_HOST_INT32(networkByteOrder);
   }

   MMClearMessage(msg);

   /* Read entries */
   uint32 i;
   for (i=0; i<numEntries; i++)
   {
      const char * fieldName;
      uint32 nameLength, tc, eLength;
      MMessageField * newField = NULL;
      MBool doAddField = MTrue;
      const uint8 * dataPtr;

      /* Read entry name length */
      if (ReadData(buffer, inputBufferBytes, &readOffset, &networkByteOrder, sizeof(networkByteOrder)) != CB_NO_ERROR) return CB_ERROR;

      nameLength = B_LENDIAN_TO_HOST_INT32(networkByteOrder);
      if (nameLength > inputBufferBytes-readOffset) return CB_ERROR;

      /* Read entry name */
      fieldName = (const char *) &buffer[readOffset];
      readOffset += nameLength;

      /* Read entry type code */
      if (ReadData(buffer, inputBufferBytes, &readOffset, &networkByteOrder, sizeof(networkByteOrder)) != CB_NO_ERROR) return CB_ERROR;
      tc = B_LENDIAN_TO_HOST_INT32(networkByteOrder);

      /* Read entry data length */
      if (ReadData(buffer, inputBufferBytes, &readOffset, &networkByteOrder, sizeof(networkByteOrder)) != CB_NO_ERROR) return CB_ERROR;
      eLength = B_LENDIAN_TO_HOST_INT32(networkByteOrder);
      if (eLength > inputBufferBytes-readOffset) return CB_ERROR;
      
      dataPtr = &buffer[readOffset];
      switch(tc)
      {
         case B_BOOL_TYPE:   newField = ImportMMessageField(fieldName, nameLength, tc, eLength, dataPtr, sizeof(MBool),  sizeof(MBool));  break;
         case B_DOUBLE_TYPE: newField = ImportMMessageField(fieldName, nameLength, tc, eLength, dataPtr, sizeof(double), sizeof(double)); break;
         case B_FLOAT_TYPE:  newField = ImportMMessageField(fieldName, nameLength, tc, eLength, dataPtr, sizeof(float),  sizeof(float));  break;
         case B_INT64_TYPE:  newField = ImportMMessageField(fieldName, nameLength, tc, eLength, dataPtr, sizeof(int64),  sizeof(int64));  break;
         case B_INT32_TYPE:  newField = ImportMMessageField(fieldName, nameLength, tc, eLength, dataPtr, sizeof(int32),  sizeof(int32));  break;
         case B_INT16_TYPE:  newField = ImportMMessageField(fieldName, nameLength, tc, eLength, dataPtr, sizeof(int16),  sizeof(int16));  break;
         case B_INT8_TYPE:   newField = ImportMMessageField(fieldName, nameLength, tc, eLength, dataPtr, sizeof(int8),   sizeof(int8));   break;

         case B_MESSAGE_TYPE:
         {
            /* Annoying special case, since this type doesn't contain a number-of-items field,   */
            /* we have to read them in to a temporary linked list and find out how many there are that way */
            MMessage * head = NULL, * tail = NULL;
            uint32 eUsed = 0;
            uint32 eOffset = readOffset;
            uint32 listLength = 0;
            c_status_t ret = CB_NO_ERROR;
            while((eUsed < eLength)&&(ret == CB_NO_ERROR))
            {
               uint32 entryLen;
               ret = CB_ERROR;  /* go pessimistic for a bit */
               if (ReadData(buffer, inputBufferBytes, &eOffset, &entryLen, sizeof(entryLen)) == CB_NO_ERROR)
               {
                  entryLen = B_LENDIAN_TO_HOST_INT32(entryLen);
                  if (eOffset + entryLen <= inputBufferBytes)
                  {
                     MMessage * newMsg = MMAllocMessage(0);
                     if (newMsg)
                     {
                        if (MMUnflattenMessage(newMsg, &buffer[eOffset], entryLen) == CB_NO_ERROR)
                        {
                           ret = CB_NO_ERROR;
                           eOffset += entryLen;
                           eUsed   += (sizeof(uint32) + entryLen);
                           if (tail) 
                           {
                              tail->scratch = newMsg;
                              tail = newMsg;
                           }
                           else head = tail = newMsg;

                           listLength++;
                        }
                        else MMFreeMessage(newMsg);
                     }
                  }
               }
            }
            if (ret == CB_NO_ERROR)
            {
               MMessage ** newMsgField = MMPutMessageField(msg, MFalse, fieldName, listLength);
               if (newMsgField)
               {
                  uint32 j;
                  for (j=0; j<listLength; j++) 
                  {
                     newMsgField[j] = head;
                     head = head->scratch;
                  }
                  doAddField = MFalse;
               }
               else ret = CB_ERROR; 
            }
            if (ret != CB_NO_ERROR)
            {
               /* Clean up on error */
               while(head)
               {
                  MMessage * next = head->scratch;
                  MMFreeMessage(head);
                  head = next; 
               }
               MMRemoveField(msg, fieldName);
               return CB_ERROR;
            }
         }
         break;

         /** Note that this should never really happen since we don't put pointers in flattened messages anymore, but it's here for backwards compatibility
           * with older code that did do that.
           */
         case B_POINTER_TYPE: newField = ImportMMessageField(fieldName, nameLength, B_INT32_TYPE, eLength, dataPtr, sizeof(int32),   sizeof(int32)); break;
         case B_POINT_TYPE:   newField = ImportMMessageField(fieldName, nameLength, tc,           eLength, dataPtr, sizeof(float)*2, sizeof(float)); break;
         case B_RECT_TYPE:    newField = ImportMMessageField(fieldName, nameLength, tc,           eLength, dataPtr, sizeof(float)*4, sizeof(float)); break;

         default: 
         {
            /* All other types get loaded as variable-sized byte buffers */
            uint32 numItems;
            uint32 eOffset = readOffset;
            if (ReadData(buffer, inputBufferBytes, &eOffset, &numItems, sizeof(numItems)) == CB_NO_ERROR)
            {
               MByteBuffer ** bufs;
               numItems = B_LENDIAN_TO_HOST_INT32(numItems);
               bufs = PutMMVariableFieldAux(msg, MFalse, tc, fieldName, numItems);
               if (bufs)
               {
                  uint32 eLeft = eLength;

                  doAddField = MFalse;
                  uint32 j=0;
                  for (j=0; j<numItems; j++)
                  {
                     uint32 itemSize; 
                     MBool ok = MFalse;
                     if (ReadData(buffer, inputBufferBytes, &eOffset, &itemSize, sizeof(itemSize)) == CB_NO_ERROR)
                     {
                        itemSize = B_LENDIAN_TO_HOST_INT32(itemSize);
                        if ((itemSize+sizeof(uint32) <= eLeft)&&((bufs[j] = MBAllocByteBuffer(itemSize, MFalse)) != NULL))
                        {
                           eLeft -= (itemSize + sizeof(uint32));
                           memcpy(&bufs[j]->bytes, &buffer[eOffset], itemSize);
                           eOffset += itemSize;
                           ok = true; 
                        }
                     }
                     if (ok == MFalse)
                     {
                        MMRemoveField(msg, fieldName);
                        return CB_ERROR;
                     }
                  }
               }
            }
         }
         break;
      }

      if (doAddField)
      {
         if (newField) AddMMessageField(msg, newField);
                  else return CB_ERROR;
      }

      readOffset += eLength;
   }
   return CB_NO_ERROR;
}

c_status_t MMMoveField(MMessage * sourceMsg, const char * fieldName, MMessage * destMsg)
{
   if (sourceMsg == destMsg) return CB_NO_ERROR;  /* easy to move it to itself, it's already there! */
   else
   {
      MMessageField * moveMe = LookupMMessageField(sourceMsg, fieldName, B_ANY_TYPE);
      if (moveMe)
      {
         DetachMMessageField(sourceMsg, moveMe);
         if (destMsg)
         {
            MMessageField * killMe = LookupMMessageField(destMsg, fieldName, B_ANY_TYPE);
            if (killMe)
            {
               DetachMMessageField(destMsg, killMe);
               FreeMMessageField(moveMe);
            }
            AddMMessageField(destMsg, moveMe);
         }
         else FreeMMessageField(moveMe);  /* move to NULL == kill it */

         return CB_NO_ERROR;
      }
      return CB_ERROR;
   }
}

c_status_t MMCopyField(const MMessage * sourceMsg, const char * fieldName, MMessage * destMsg)
{
   if (sourceMsg == destMsg) return CB_NO_ERROR;  /* easy to move it to itself, it's already there! */
   else
   {
      MMessageField * copyMe = LookupMMessageField(sourceMsg, fieldName, B_ANY_TYPE);
      if (copyMe)
      {
         if (destMsg)
         {
            MMessageField * clone = CloneMMessageField(copyMe);
            if (clone)
            {
               MMessageField * killMe = LookupMMessageField(destMsg, fieldName, B_ANY_TYPE);
               if (killMe)
               {
                  DetachMMessageField(destMsg, killMe);
                  FreeMMessageField(killMe);
               }
               AddMMessageField(destMsg, clone);
            }
            else return CB_ERROR;
         }
         return CB_NO_ERROR;
      }
      return CB_ERROR;
   }
}

c_status_t MMRenameField(MMessage * msg, const char * oldFieldName, const char * newFieldName)
{
   if (strcmp(oldFieldName, newFieldName) == 0) return CB_NO_ERROR;
   else
   {
      MMessageField * f = LookupMMessageField(msg, oldFieldName, B_ANY_TYPE);
      if (f)
      {
         MMessageField * overwriteThis = LookupMMessageField(msg, newFieldName, B_ANY_TYPE);  /* do this lookup before any mods! */
         const int slen = ((int)strlen(newFieldName))+1;
         const int diff = (slen - f->nameBytes);
         if (diff != 0)
         {
            const uint32 newSize = f->allocSize+diff;
            MMessageField * g = (MMessageField *) MRealloc(f, newSize);
            if (g == NULL) return CB_ERROR;

            g->allocSize = newSize;
            g->nameBytes = slen;
            if (g != f)
            {
               /* Oops, gotta update all the pointers to point to the new field */
               g->data = ((char *)g) + WITH_ALIGNMENT_PADDING(sizeof(MMessageField));  /* data bytes start immediately after the struct */
               g->name = ((const char *)g->data)+(g->numItems*g->itemSize);    /* name starts right after the data */
               if (g->prevField) g->prevField->nextField = g;
               if (g->nextField) g->nextField->prevField = g;
               if (msg->firstField == f) msg->firstField = g;
               if (msg->lastField  == f) msg->lastField  = g;
               f = g;
            }
         }
         memcpy((char *)f->name, newFieldName, slen);

         /* If our field now occupies the same name as another, delete the other, to maintain fieldname uniqueness */
         if (overwriteThis) 
         {
            DetachMMessageField(msg, overwriteThis);
            FreeMMessageField(overwriteThis);
         }

         return CB_NO_ERROR;
      }
   }
   return CB_ERROR;
}
   
MByteBuffer ** MMGetStringField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems)
{
   return (MByteBuffer **) GetFieldDataAux(msg, fieldName, B_STRING_TYPE, optRetNumItems);
}

MBool * MMGetBoolField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems)
{
   return (MBool *) GetFieldDataAux(msg, fieldName, B_BOOL_TYPE, optRetNumItems);
}

int8 * MMGetInt8Field(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems)
{
   return (int8 *) GetFieldDataAux(msg, fieldName, B_INT8_TYPE, optRetNumItems);
}

int16 * MMGetInt16Field(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems)
{
   return (int16 *) GetFieldDataAux(msg, fieldName, B_INT16_TYPE, optRetNumItems);
}

int32 * MMGetInt32Field(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems)
{
   return (int32 *) GetFieldDataAux(msg, fieldName, B_INT32_TYPE, optRetNumItems);
}

int64 * MMGetInt64Field(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems)
{
   return (int64 *) GetFieldDataAux(msg, fieldName, B_INT64_TYPE, optRetNumItems);
}

float * MMGetFloatField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems)
{
   return (float *) GetFieldDataAux(msg, fieldName, B_FLOAT_TYPE, optRetNumItems);
}

double * MMGetDoubleField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems)
{
   return (double *) GetFieldDataAux(msg, fieldName, B_DOUBLE_TYPE, optRetNumItems);
}

MMessage ** MMGetMessageField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems)
{
   return (MMessage **) GetFieldDataAux(msg, fieldName, B_MESSAGE_TYPE, optRetNumItems);
}

void ** MMGetPointerField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems)
{
   return (void **) GetFieldDataAux(msg, fieldName, B_POINTER_TYPE, optRetNumItems);
}

MPoint * MMGetPointField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems)
{
   return (MPoint *) GetFieldDataAux(msg, fieldName, B_POINT_TYPE, optRetNumItems);
}

MRect * MMGetRectField(const MMessage * msg, const char * fieldName, uint32 * optRetNumItems)
{
   return (MRect *) GetFieldDataAux(msg, fieldName, B_RECT_TYPE, optRetNumItems);
}

MByteBuffer ** MMGetDataField(const MMessage * msg, uint32 typeCode, const char * fieldName, uint32 * optRetNumItems)
{
   return ((typeCode != B_MESSAGE_TYPE)&&(IsTypeCodeVariableSize(typeCode))) ? (MByteBuffer **) GetFieldDataAux(msg, fieldName, typeCode, optRetNumItems) : NULL;
}

c_status_t MMGetFieldInfo(const MMessage * msg, const char * fieldName, uint32 typeCode, uint32 * optRetNumItems, uint32 * optRetTypeCode)
{
   MMessageField * f = LookupMMessageField(msg, fieldName, typeCode);
   if (f)
   {
      if (optRetNumItems) *optRetNumItems = f->numItems; 
      if (optRetTypeCode) *optRetTypeCode = f->typeCode;
      return CB_NO_ERROR;
   }
   return CB_ERROR;
}

MMessageIterator MMGetFieldNameIterator(const MMessage * msg, uint32 typeCode)
{
   MMessageIterator ret; 
   ret.message   = msg;
   ret.iterState = msg->firstField;
   ret.typeCode  = typeCode;
   return ret;
}

const char * MMGetNextFieldName(MMessageIterator * iter, uint32 * optRetTypeCode)
{
   while(iter->iterState)
   {
      MMessageField * f = (MMessageField *) iter->iterState;
      if ((iter->typeCode == B_ANY_TYPE)||(iter->typeCode == f->typeCode))
      {
         const char * ret = f->name;
         if (optRetTypeCode) *optRetTypeCode = f->typeCode;
         iter->iterState = f->nextField;
         return ret;
      }
      else iter->iterState = f->nextField;
   }
   return NULL; 
}

static void DoIndent(int numSpaces)
{
   int i=0;
   for (i=0; i<numSpaces; i++) putchar(' ');
}

static void PrintMMessageToStreamAux(const MMessage * msg, FILE * file, int indent);  /* forward declaration */

static void PrintMMessageFieldToStream(const MMessageField * field, FILE * file, int indent)
{
   char pbuf[5];
   const void * data = field->data;

   int i = 0;
   int ni = field->numItems;
   if (ni > 10) ni = 10;  /* truncate to avoid too much spam */

   MakePrettyTypeCodeString(field->typeCode, pbuf);
   DoIndent(indent); fprintf(file, "Entry: Name=[%s] GetNumItems()=" UINT32_FORMAT_SPEC ", TypeCode=%s (" INT32_FORMAT_SPEC ") flatSize=" UINT32_FORMAT_SPEC "\n", field->name, field->numItems, pbuf, field->typeCode, GetMMessageFieldFlattenedSize(field, MFalse));
   for (i=0; i<ni; i++)
   {
      DoIndent(indent); fprintf(file, "  %i. ", i);
      switch(field->typeCode)
      {
         case B_BOOL_TYPE:    fprintf(file, "%i\n",                 (((const MBool *)data)[i])); break;
         case B_DOUBLE_TYPE:  fprintf(file, "%f\n",                 ((const double *)data)[i]);  break;
         case B_FLOAT_TYPE:   fprintf(file, "%f\n",                 ((const float *)data)[i]);   break;
         case B_INT64_TYPE:   fprintf(file, INT64_FORMAT_SPEC "\n", ((const int64 *)data)[i]);   break;
         case B_INT32_TYPE:   fprintf(file, INT32_FORMAT_SPEC "\n", ((const int32 *)data)[i]);   break;
         case B_POINTER_TYPE: fprintf(file, "%p\n",                 ((const void **)data)[i]);   break;
         case B_INT16_TYPE:   fprintf(file, "%i\n",                 ((const int16 *)data)[i]);   break;
         case B_INT8_TYPE:    fprintf(file, "%i\n",                 ((const int8 *)data)[i]);    break;

         case B_POINT_TYPE:
         {
            const MPoint * pt = &((const MPoint *)data)[i];
            fprintf(file, "x=%f y=%f\n", pt->x, pt->y);
         }
         break;

         case B_RECT_TYPE:
         {
            const MRect * rc = &((const MRect *)data)[i];
            fprintf(file, "l=%f t=%f r=%f b=%f\n", rc->left, rc->top, rc->right, rc->bottom);
         }
         break;

         case B_MESSAGE_TYPE:
         {
            MMessage * subMsg = ((MMessage **)field->data)[i];
            if (subMsg) PrintMMessageToStreamAux(subMsg, file, indent+3);
                   else fprintf(file, "(NULL Message)\n");
         }
         break;

         case B_STRING_TYPE:
         {
            MByteBuffer * subBuf = ((MByteBuffer **)field->data)[i];
            if (subBuf) fprintf(file, "[%s]\n", (const char *) (&subBuf->bytes));
                   else fprintf(file, "(NULL String)\n");
         }
         break;

         default:
         {
            MByteBuffer * subBuf = ((MByteBuffer **)field->data)[i];
            if (subBuf)
            {
               int numBytes = subBuf->numBytes;
               if (numBytes > 0)
               {
                  const uint8 * b = &subBuf->bytes;
                  int nb = subBuf->numBytes;
                  int j;

                  if (nb > 10)
                  { 
                     fprintf(file, "(%i bytes, starting with", nb);
                     nb = 10;
                  }
                  else fprintf(file, "(%i bytes, equal to",nb);

                  for (j=0; j<nb; j++) fprintf(file, " %02x", b[j]);
                  if (nb < subBuf->numBytes) fprintf(file, "...)\n");
                                        else fprintf(file, ")\n");
               }
               else fprintf(file, "(zero-length buffer)\n");
            }
            else fprintf(file, "(NULL Buffer)\n");
         }
         break;
      }
   }
}

static void PrintMMessageToStreamAux(const MMessage * msg, FILE * file, int indent)
{
   /* Make a pretty type code string */
   char buf[5];
   MakePrettyTypeCodeString(msg->what, buf);

   fprintf(file, "MMessage:  msg=%p, what='%s' (" INT32_FORMAT_SPEC "), entryCount=" INT32_FORMAT_SPEC ", flatSize=" UINT32_FORMAT_SPEC "\n", msg, buf, msg->what, msg->numFields, MMGetFlattenedSize(msg));

   indent += 2;
   {
      MMessageField * f = msg->firstField;
      while(f)
      {
         PrintMMessageFieldToStream(f, file, indent);
         f = f->nextField;
      }
   }
}

void MMPrintToStream(const MMessage * msg, FILE * optFile)
{
   PrintMMessageToStreamAux(msg, optFile?optFile:stdout, 0);
}

MBool MMAreMessagesEqual(const MMessage * msg1, const MMessage * msg2)
{
   if (msg1 == msg2) return MTrue;
   if ((msg1->what != msg2->what)||(msg1->numFields != msg2->numFields)) return MFalse;
   else
   {
      const MMessageField * f1 = msg1->firstField;
      while(f1)
      {
         uint32 numItems = f1->numItems;
         const MMessageField * f2 = LookupMMessageField(msg2, f1->name, f1->typeCode);
         if ((f2 == NULL)||(f2->numItems != numItems)||(f2->typeCode != f1->typeCode)) return MFalse;

         if (IsTypeCodeVariableSize(f1->typeCode))
         {
            uint32 i;
            if (f1->typeCode == B_MESSAGE_TYPE)
            {
               const MMessage ** msgs1 = (const MMessage **) f1->data;
               const MMessage ** msgs2 = (const MMessage **) f2->data;
               for (i=0; i<numItems; i++) if (((msgs1[i] != NULL) != (msgs2[i] != NULL))||((msgs1[i])&&(MMAreMessagesEqual(msgs1[i], msgs2[i]) == MFalse))) return MFalse;
            }
            else
            {
               const MByteBuffer ** bufs1 = (const MByteBuffer **) f1->data;
               const MByteBuffer ** bufs2 = (const MByteBuffer **) f2->data;
               for (i=0; i<numItems; i++) if (((bufs1[i] != NULL) != (bufs2[i] != NULL))||((bufs1[i])&&(MBAreByteBuffersEqual(bufs1[i], bufs2[i]) == MFalse))) return MFalse;
            }
         }
         else if (memcmp(f1->data, f2->data, numItems*f1->itemSize) != 0) return MFalse;

         f1 = f1->nextField;
      }
   }
   return MTrue; 
}

#ifdef cplusplus
};
#endif
