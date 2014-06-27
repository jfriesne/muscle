#include "dataio/MultiDataIO.h"

namespace muscle {
 
int32 MultiDataIO :: Read(void * buffer, uint32 size)
{
   if (HasChildren())
   {
      int32 ret = GetFirstChild()->Read(buffer, size);
      if (ret < 0) 
      {
         if ((_absorbPartialErrors)&&(_childIOs.GetNumItems() > 1))
         {
            (void) _childIOs.RemoveHead();
            return Read(buffer, size);  // try again with the new first child
         }
         else return -1;
      }
      else if (ret > 0) return (SeekAll(1, ret, IO_SEEK_CUR)==B_NO_ERROR) ? ret : -1;
   }
   return 0;
}

int32 MultiDataIO :: Write(const void * buffer, uint32 size) 
{
   int64 newSeekPos       = -1;  // just to shut the compiler up
   uint32 maxWrittenBytes = 0;
   uint32 minWrittenBytes = MUSCLE_NO_LIMIT;
   for (int32 i=_childIOs.GetNumItems()-1; i>=0; i--)
   {
      int32 childRet = _childIOs[i]()->Write(buffer, muscleMin(size, minWrittenBytes));
      if (childRet < 0) 
      {
         if ((_absorbPartialErrors)&&(_childIOs.GetNumItems() > 1)) (void) _childIOs.RemoveItemAt(i);
                                                               else return -1;
      }
      else
      {
         if ((uint32)childRet < minWrittenBytes) 
         {
            minWrittenBytes = childRet;
            newSeekPos      = _childIOs[i]()->GetPosition();
         }
         maxWrittenBytes = muscleMax(maxWrittenBytes, (uint32)childRet);
      }
   }

   if (minWrittenBytes < maxWrittenBytes)
   {
      // Oh dear, some children wrote more bytes than others.  To make their seek-positions equal again,
      // we are going to seek everybody to the seek-position of the child that wrote the fewest bytes.
      if (SeekAll(0, newSeekPos, IO_SEEK_CUR) != B_NO_ERROR) return -1;
   }

   return (maxWrittenBytes > 0) ? minWrittenBytes : 0;  // the conditional is there in case minWrittenBytes is still MUSCLE_NO_LIMIT
}

void MultiDataIO :: FlushOutput() 
{
   for (int32 i=_childIOs.GetNumItems()-1; i>=0; i--) _childIOs[i]()->FlushOutput();
}

void MultiDataIO :: WriteBufferedOutput() 
{
   for (int32 i=_childIOs.GetNumItems()-1; i>=0; i--) _childIOs[i]()->WriteBufferedOutput();
}

uint32 MultiDataIO :: GetPacketMaximumSize() const
{
   return (HasChildren()) ? GetFirstChild()->GetPacketMaximumSize() : 0;
}

bool MultiDataIO :: HasBufferedOutput() const 
{
   for (int32 i=_childIOs.GetNumItems()-1; i>=0; i--) if (_childIOs[i]()->HasBufferedOutput()) return true;
   return false;
}

status_t MultiDataIO :: SeekAll(uint32 first, int64 offset, int whence)
{
   for (int32 i=_childIOs.GetNumItems()-1; i>=(int32)first; i--)
   {
      if (_childIOs[i]()->Seek(offset, whence) != B_NO_ERROR) 
      {
         if ((_absorbPartialErrors)&&(_childIOs.GetNumItems() > 1)) (void) _childIOs.RemoveItemAt(i);
                                                               else return B_ERROR;
      }
   }
   return B_NO_ERROR;
}

}; // end namespace muscle
