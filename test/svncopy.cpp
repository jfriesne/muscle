#include <stdio.h>
#include "util/Queue.h"
#include "util/String.h"

using namespace muscle;

static void FlushAdds(String & s)
{
   if (s.HasChars())
   {
      printf("svn add %s\n", s());
      s.Clear();
   }
}

/** This program takes a list of file paths on stdin and an input directory as an argument,
  * and creates a set of SVN commands that will update a SVN repository to contain the files
  * and directories listed on stdin.
  * The list of files can be created via "tar tf archive.tar", etc.
  *
  * Note that for new repositories, svn import can do the same job as this utility; probably
  * better.  But this utility is useful when you need to bulk-upgrade an existing SVN 
  * repository from a non-SVN archive (e.g. if you are keeping 3rd party code in SVN
  * and want to upgrade your SVN repository to the new release)
  *
  * Note that this script does NOT handle the removal of obsolete files from your SVN
  * repository.  If you care about that, you'll have to do it by hand.
  *
  * -Jeremy Friesner (jaf@meyersound.com)
  *
  */
int main(int argc, char ** argv)
{
   if (argc < 2)
   {
      printf("Usage:  svncopy input_folder <filelist.txt\n");
      return 10;
   }

   String inPath = argv[1];
   if (inPath.EndsWith("/") == false) inPath += '/';
   printf("#!/bin/sh\n");
   printf("# Creating commands to copy files from input folder [%s]\n\n", inPath());

   Queue<String> mkdirs;
   Queue<String> copies;

   char buf[2048];
   while(fgets(buf, sizeof(buf), stdin))
   {
      String next(buf);
      next=next.Trim();
      next.Replace("\'", "\\\'");
      if (next.EndsWith("/")) mkdirs.AddTail(next);
                         else copies.AddTail(next);
   }

   for (uint32 i=0; i<mkdirs.GetNumItems(); i++) printf("mkdir \"./%s\"\n", mkdirs[i]());
   for (uint32 i=0; i<copies.GetNumItems(); i++) printf("cp \"%s%s\" \"./%s\"\n", inPath(), copies[i](), copies[i]());
  
   // Directory adds must be done separately, since if some fail we want the others to continue
   for (uint32 i=0; i<mkdirs.GetNumItems(); i++) printf("svn add \"./%s\"\n", mkdirs[i]());

   // File adds can be batched together, since already-present files won't cause the whole command to fail
   String temp;
   for (uint32 i=0; i<copies.GetNumItems(); i++)
   {
      const int MAX_LINE_LENGTH = 2048;
      const String & next = copies[i];
      if (next.Length() + temp.Length() >= MAX_LINE_LENGTH-10) FlushAdds(temp);
      temp += String("\"./%1\" ").Arg(next);
   } 
   FlushAdds(temp);

   return 0;
}
