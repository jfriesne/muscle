/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "regex/StringMatcher.h"
#include "system/SystemInfo.h"  // for GetFilePathSeparator()
#include "util/Directory.h"
#include "util/FilePathInfo.h"
#include "util/String.h"

namespace muscle {

status_t ExpandFilePathWildCardsAux(const String & curDir, const String & path, Queue<String> & outputPaths, bool isSimpleFormat)
{
   const char * fs = GetFilePathSeparator();
   int32 sepIdx = path.IndexOf(fs);
   String firstClause  = (sepIdx >= 0) ? path.Substring(0, sepIdx) : path;
   String restOfString = (sepIdx >= 0) ? path.Substring(sepIdx+1) : GetEmptyString();

   StringMatcher sm(firstClause, isSimpleFormat);
   Directory dir(curDir());
   if (dir.IsValid())
   {
      if (CanWildcardStringMatchMultipleValues(firstClause))
      {
         while(1)
         {
            const char * fn = dir.GetCurrentFileName();
            if (fn)
            {
               if ((strcmp(fn, ".") != 0)&&(strcmp(fn, "..") != 0)&&((fn[0] != '.')||(firstClause.StartsWith(".")))&&(sm.Match(fn)))
               {
                  String childPath = String(dir.GetPath())+fn;
                  if (restOfString.HasChars())
                  {
                     if (ExpandFilePathWildCardsAux(childPath, restOfString, outputPaths, isSimpleFormat) != B_NO_ERROR) return B_ERROR;
                  }
                  else if (outputPaths.AddTail(childPath) != B_NO_ERROR) return B_ERROR;
               }
               dir++;
            }
            else break;
         }
      }
      else
      {
         String childPath = String(dir.GetPath())+firstClause;
         if (FilePathInfo(childPath()).Exists())
         {
            if (restOfString.HasChars())
            {
               if (ExpandFilePathWildCardsAux(childPath, restOfString, outputPaths, isSimpleFormat) != B_NO_ERROR) return B_ERROR;
            }
            else if (outputPaths.AddTail(childPath) != B_NO_ERROR) return B_ERROR;
         }
      }
   }
   return B_NO_ERROR;
}

status_t ExpandFilePathWildCards(const String & path, Queue<String> & outputPaths, bool isSimpleFormat)
{
   const char * fs = GetFilePathSeparator();
   if (path.StartsWith(fs)) return ExpandFilePathWildCardsAux(fs,  path()+1, outputPaths, isSimpleFormat);  // start at root
                       else return ExpandFilePathWildCardsAux(".", path,     outputPaths, isSimpleFormat);  // start at current folder
}

}; // end namespace muscle
