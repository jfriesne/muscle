/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "besupport/ConvertMessages.h"

namespace muscle {

status_t ConvertToBMessage(const Message & from, BMessage & to) 
{
   to.MakeEmpty();
   to.what = from.what;

   uint32 type;
   uint32 count;
   bool fixedSize;

   for (MessageFieldNameIterator it = from.GetFieldNameIterator(B_ANY_TYPE, HTIT_FLAG_NOREGISTER); it.HasData(); it++)
   {
      const String & n = it.GetFieldName();
      if (from.GetInfo(n, &type, &count, &fixedSize) != B_NO_ERROR) return B_ERROR;

      for (uint32 j=0; j<count; j++)
      {
         const void * nextItem;
         uint32 itemSize;
         if (from.FindData(n, type, j, &nextItem, &itemSize) != B_NO_ERROR) return B_ERROR;

         // do any necessary translation from the Muscle data types to Be data types
         switch(type)
         {
            case B_POINT_TYPE:
            {
               const Point * p = static_cast<const Point *>(nextItem);
               BPoint bpoint(p->x(), p->y());
               if (to.AddPoint(n(), bpoint) != B_NO_ERROR) return B_ERROR;
            }
            break;

            case B_RECT_TYPE:
            {
               const Rect * r = static_cast<const Rect *>(nextItem);
               BRect brect(r->left(), r->top(), r->right(), r->bottom());
               if (to.AddRect(n(), brect) != B_NO_ERROR) return B_ERROR;
            }
            break;
 
            case B_MESSAGE_TYPE:
            {
               const MessageRef * msgRef = static_cast<const MessageRef *>(nextItem);
               BMessage bmsg;
               if (msgRef->GetItemPointer() == NULL) return B_ERROR;
               if (ConvertToBMessage(*msgRef->GetItemPointer(), bmsg) != B_NO_ERROR) return B_ERROR;
               if (to.AddMessage(n(), &bmsg) != B_NO_ERROR) return B_ERROR;
            }
            break;

            default:
               if (to.AddData(n(), type, nextItem, itemSize, fixedSize, count) != B_NO_ERROR) return B_ERROR;
            break;
         }
      }
   }
   return B_NO_ERROR;
}

status_t ConvertFromBMessage(const BMessage & from, Message & to) 
{
   to.Clear();
   to.what = from.what;

#if B_BEOS_VERSION_DANO
   const char * name;
#else
   char * name;
#endif

   type_code type;
   int32 count;

   for (int32 i=0; (from.GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_NO_ERROR); i++)
   {
      for (int32 j=0; j<count; j++)
      {
         const void * nextItem;
         int32 itemSize;
         if (from.FindData(name, type, j, &nextItem, &itemSize) != B_NO_ERROR) return B_ERROR;

         // do any necessary translation from the Be data types to Muscle data types
         switch(type)
         {
            case B_POINT_TYPE:
            {
               const BPoint * p = static_cast<const BPoint *>(nextItem);
               Point pPoint(p->x, p->y);
               if (to.AddPoint(name, pPoint) != B_NO_ERROR) return B_ERROR;
            }
            break;

            case B_RECT_TYPE:
            {
               const BRect * r = static_cast<const BRect *>(nextItem);
               Rect pRect(r->left, r->top, r->right, r->bottom);
               if (to.AddRect(name, pRect) != B_NO_ERROR) return B_ERROR;
            }
            break;

            case B_MESSAGE_TYPE:
            {
               BMessage bmsg;
               if (bmsg.Unflatten(static_cast<const char *>(nextItem)) != B_NO_ERROR) return B_ERROR;
               Message * newMsg = newnothrow Message;
               MRETURN_OOM_ON_NULL(newMsg);

               MessageRef msgRef(newMsg);
               if (ConvertFromBMessage(bmsg, *newMsg) != B_NO_ERROR) return B_ERROR;
               if (to.AddMessage(name, msgRef) != B_NO_ERROR) return B_ERROR;
            }
            break;

            default:
               if (to.AddData(name, type, nextItem, itemSize) != B_NO_ERROR) return B_ERROR;
            break;
         }
      }
   }
   return B_NO_ERROR;
}

} // end namespace muscle
