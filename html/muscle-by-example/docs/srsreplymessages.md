## Result Messages Page Overview

* This page describes the semantics of some selected [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) what-codes that a [StorageReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSession.html) may send (via TCP) to its client.
* These what-codes (and their associated PR_NAME_* field-name strings) are defined in [reflector/StorageReflectConstants.h](https://public.msli.com/lcs/muscle/html/StorageReflectConstants_8h.html)
* See also the description in the header comments, beginning at line 139 of [StorageReflectConstants.h](https://public.msli.com/lcs/muscle/html/StorageReflectConstants_8h_source.html)
* Note that the parsing examples shown below would typically be seen in client-side code (because on the server-side, there are virtual methods of the [StorageReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSession.html) class that you would override, which would be easier and more efficient than hand-parsing a reply-[Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) object that had just been created locally, just to get the data back out of it)

## Result codes

This is a partial list -- the full list of result codes can be seen [here](https://public.msli.com/lcs/muscle/html/StorageReflectConstants_8h.html#a385c44f6fb256e5716a2302a5b940388ac85a30ca3ed035e4f43ed315597553fc).

- `PR_RESULT_DATAITEMS` - *Sent to client in response to a `PR_COMMAND_GETDATA` or as a subscription update*
    - This is the primary Message-type that [StorageReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSession.html) uses to update a client about changes to subscribed nodes in the server-side database
    - This [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) will contain either or both of two types of update:  A list of node-paths of nodes that have been deleted, and a list of node-paths whose Message payloads have been created/updated.
    - An example of how a client might parse a [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) of this type follows:

<pre>
   void ParseIncomingDataItemsMessage(const MessageRef & msg)
   {
      if (msg()->what != PR_RESULT_DATAITEMS) return;  // sanity check

      const String * deletedNodePath;
      for (int i=0; (msg()->FindString(PR_NAME_REMOVED_DATAITEMS, i, &deletedNodePath) == B_NO_ERROR); i++)
      {
         printf("Subscribed node at path [%s] was deleted!\n", deletedNodePath->Cstr());
      }

      for (MessageFieldNameIterator iter = msg()->GetFieldNameIterator(B_MESSAGE_TYPE); iter.HasData(); iter++)
      {
         const String & np = iter.GetFieldName();
         MessageRef dataMsg;
         for (int32 i=0; msg()->FindMessage(np, i, dataMsg) == B_NO_ERROR; i++)
         {
            printf("Subscribed node at path [%s] was updated!\n", np());
            printf("New Message payload for that path is:\n");
            dataMsg()->PrintToStream();
         }
      }
   }
</pre>

- `PR_RESULT_INDEXUPDATED` - *Notification that a node's ordered-children-index has been changed*
    - All of the fields of this [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) will be strings
    - The name of each field is the path of a [DataNode](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) whose ordered-children-index has changed
    - The string-values in the field are descriptions of how it has changed
    - A string-value equal to "c" (aka `INDEX_OP_CLEARED`) indicates that the index has been cleared/erased
    - A string-value starting with "i" (aka `INDEX_OP_ENTRYINSERTED`) indicates that a node has been inserted into the index at a specified location.  For example, "i43:blah" inserts that the child node named "blah" has been inserted at offset 43 of the index (note that 0 is the first valid offset).
    - A string-value starting with "r" (aka `INDEX_OP_ENTRYREMOVED`) indicates that a node has been inserted into the index at a specified location.  For example, "r2:blah" means that the child node named "blah" has been removed from its location at offset 2 of the index (i.e. from the third position in the list)
    - A client would typically update a local data structure according to these strings' instructions, in order to keep it in sync with the index's data structure on the server.
    - An example of how a client might parse a [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) of this type follows:

<pre>  
   // This table holds the child-node-names-in-index-order list for each 
   // ordered-index-node that the client is subscribed to
   Hashtable&lt;String, Queue&lt;String&gt; &gt; indices;  // nodepath -> index

   void ParseIncomingIndexUpdatedMessage(const MessageRef & msg)
   {
      if (msg()->what != PR_RESULT_INDEXUPDATED) return;  // sanity check

      for (MessageFieldNameIterator iter = msg()->GetFieldNameIterator(B_STRING_TYPE); iter.HasData(); iter++)
      {
         const String & parentNodePath = iter.GetFieldName();
         printf("Parsing index updates for DataNode at [%s]\n", parentNodePath());

         const String * nextInstruction;
         for (int32 i=0; msg()->FindString(parentNodePath, i, nextInstruction) == B_NO_ERROR; i++)
         {
            const char * childName = strchr(nextInstruction->Cstr(), ':');
            if (childName) childName++;  // child node name starts after the colon

            const int indexPos = atoi(nextInstruction->Cstr()+1);
            switch(nextInstruction()->Cstr()[0])
            {
               case INDEX_OP_CLEARED:  // a.k.a 'c'
                  (void) indices.Remove(parentNodePath);
               break;

               case INDEX_OP_ENTRYINSERTED:   // a.k.a 'i'
               {
                  Queue&lt;String&gt; * idx = indices.GetOrPut(parentNodePath);
                  if (idx) (void) idx->InsertItemAt(pos, childName);
               }
               break;
 
               case INDEX_OP_ENTRYREMOVED:   // a.k.a 'r'
               {
                  Queue&lt;String&gt; * idx = indices.GetOrPut(parentNodePath);
                  if ((idx)&&(idx->RemoveItemAt(pos) == B_NO_ERROR)&&(idx->IsEmpty()))
                  {
                     (void) indices.Remove(parentNodePath);
                  }
               }
               break;
            } 
         }
      }
   }
</pre>

Quick links to source code of relevant MUSCLE-by-example programs:

* [reflector/example_4_smart_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_4_smart_server.cpp)
* [reflector/example_5_smart_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_5_smart_client.cpp)
* [test/portablereflectclient.cpp](https://public.msli.com/lcs/muscle/muscle/test/portablereflectclient.cpp)
* [test/chatclient.cpp](https://public.msli.com/lcs/muscle/muscle/test/chatclient.cpp)
* [qtsupport/qt_example/qt_example.cpp](https://public.msli.com/lcs/muscle/muscle/qtsupport/qt_example/qt_example.cpp)
