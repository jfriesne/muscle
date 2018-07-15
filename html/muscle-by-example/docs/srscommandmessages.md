## Command Messages Page Overview

* This page describes the semantics of some selected [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) what-codes that a [StorageReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSession.html) will accept (via TCP) from its client.
* These what-codes (and their associated PR_NAME_* field-name strings) are defined in [reflector/StorageReflectConstants.h](https://public.msli.com/lcs/muscle/html/StorageReflectConstants_8h.html)
* See also the description in the header comments, beginning at line 139 of [StorageReflectConstants.h](https://public.msli.com/lcs/muscle/html/StorageReflectConstants_8h_source.html)
* Note that the invocation examples shown below would typically be seen in client-side code (because on the server-side, there are methods of the [StorageReflectSession](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSession.html) class that you would just call directly, which would be easier and more efficient than hand-constructing a [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) object with the necessary fields and then calling [MessageReceivedFromGateway()](https://public.msli.com/lcs/muscle/html/classmuscle_1_1StorageReflectSession.html#a627414e26cbf6869142f10f7f8f5e5bd) with it)

## Command codes

This is a partial list -- the full list of command codes can be seen [here](https://public.msli.com/lcs/muscle/html/StorageReflectConstants_8h.html#ab04a0655cd1e3bcac5e8f48c18df1a57acf862fe04d3f070f2739c6631328c79c).

* `PR_COMMAND_PING` - *Server will echo this message, verbatim (except for the what-code) back to the sending client as a `PR_RESULT_PONG` reply Message*
    - This can be useful if the client wants to make sure the server is still alive and responding
    - It's also useful as a way of getting a notification when previous operations have completed on the server (since the server will handle all commands in FIFO order)
    - Here is an example invocation:
<pre>
MessageRef pingMsg = GetMessageFromPool(PR_COMMAND_PING);
pingMsg()->AddString("put_whatever_fields_you_like", "in_the_ping_message");
pingMsg()->AddString("they_will_come_back_to_you", "in_the_corresponding_pong_message");
myMessageTransceiverThread.SendMessageToSessions(pingMsg);
</pre>
* `PR_COMMAND_SETDATA` - *Uploads the given Message(s) to nodes at the specified path(s) the session's local node-subtree*
    - A client would send this when it wants to publish one or more [DataNodes](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) to its local subtree of the database
    - If a [DataNode](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) already exists at the specified path, its [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) payload is updated -- otherwise a [DataNode](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) with the given [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) is created there.
    - If the parent "directories" of the specified path do not exist, they will be created (with default/empty [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) payloads) as necessary.
    - Note that clients can only set nodes in their session's local subtree; they aren't allowed to modify the subtrees of other sessions.
    - The fields of this [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) should have relative-paths as their names, and [Messages](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) as their values; each field represents one [DataNode](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) to post.  
    - Here is an example invocation:
<pre>
MessageRef setDataMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
setDataMsg()->AddMessage("nodename1", GetMessageFromPool(1234));
setDataMsg()->AddMessage("nodename2", GetMessageFromPool(1234));
setDataMsg()->AddMessage("blah", GetMessageFromPool(1234));
setDataMsg()->AddMessage("subfolder/nodename3", GetMessageFromPool(1234));
myMessageTransceiverThread.SendMessageToSessions(setDataMsg);
</pre>

* `PR_COMMAND_GETDATA` - *Requests that the server send back a `PR_RESULT_DATAITEMS` Message containing the contents of the specified path(s) in the node-tree*
    - If you are using live-subscriptions, this command is generally not necessary (since you'll get automatic updates anyway)
    - Clients can use this to download the current [Messages](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) of nodes in their own session's subtree or in other sessions' subtrees
    - If for some reason you don't want to subscribe, however, you can use this to force a one-shot retrieval of matching nodes
    - Here is an example invocation:
<pre>
MessageRef getDataMsg = GetMessageFromPool(PR_COMMAND_GETDATA);
getDataMsg()->AddString(PR_NAME_KEYS, "/\*/\*/blah");  // absolute path
getDataMsg()->AddString(PR_NAME_KEYS, "node*");      // relative path
getDataMsg()->AddString(PR_NAME_KEYS, "sub*/node*");
myMessageTransceiverThread.SendMessageToSessions(getDataMsg);
</pre>

* `PR_COMMAND_REMOVEDATA` - *Deletes the data nodes from the session's subtree whose paths match the patterns in the Message*
    - Use this to delete nodes in your subtree that you want gone
    - Note that clients can only delete nodes in their own session's subtree; they aren't allowed to modify the subtrees of other sessions.
    - Here is an example invocation:
<pre>
MessageRef rmvDataMsg = GetMessageFromPool(PR_COMMAND_REMOVEDATA);
rmvDataMsg()->AddString(PR_NAME_KEYS, "blah");
rmvDataMsg()->AddString(PR_NAME_KEYS, "node*");
rmvDataMsg()->AddString(PR_NAME_KEYS, "subfolder");  // implicitly deletes children of subfolder also
myMessageTransceiverThread.SendMessageToSessions(rmvDataMsg);
</pre>
* `PR_COMMAND_SETPARAMETERS` - *Sets parameters on the session based on the fields in the Message*
    - A session's parameters govern its behavior.
    - In particular, parameters whose names start with the prefix "SUBSCRIBE:" tell the session to subscribe to whichever [DataNodes](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) have paths that match the path in the remainder of the parameters name (e.g. setting a parameter named "SUBSCRIBE:/\*/\*" would tell the session to subscribe to all [DataNodes](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) whose paths match "/\*/\*", which is a good way for the client to be informed whenever another client connects or disconnects)
    - Here is an example invocation:
<pre>
MessageRef setPMsg = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);
setPMsg()->AddBool("SUBSCRIBE:/\*/\*", true);  // type and value of this field don't matter
setPMsg()->AddBool("SUBSCRIBE:nodename3", true); // relative path, equivalent to "/*/*/nodename3"
setPMsg()->AddInt32(PR_NAME_REPLY_ENCODING, MUSCLE_MESSAGE_ENCODING_ZLIB_9);
myMessageTransceiverThread.SendMessageToSessions(setPMsg);
</pre>
* `PR_COMMAND_GETPARAMETERS` - *Requests that the server send the session's current parameters-set back to the client, via a `PR_RESULT_PARAMETERS` reply Message*
    - This [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) takes no parameters (i.e. there's no point adding any fields to a `PR_COMMAND_GETPARAMETERS` Message)
    - Here is an example invocation:
<pre>
MessageRef getPMsg = GetMessageFromPool(PR_COMMAND_GETPARAMETERS);
myMessageTransceiverThread.SendMessageToSessions(getPMsg);
</pre>
* `PR_COMMAND_REMOVEPARAMETERS` - *deletes any session parameters whose names match a pattern specified in the `PR_NAME_KEYS` String-field*
    - This is typically used to unsubscribe from previous subscriptions
    - Names of parameters to unsubscribe to are places in the `PR_NAME_KEYS` field 
    - Note that the strings in the `PR_NAME_KEYS` field will be treated as wildcard-patterns (e.g. "*" will remove all user-settable parameters)
    - Therefore, if you want to only match a literal path string (and that path string has wildcard chars in it), you may need to escape those wildcard chars to avoid matching other parameter names by mistake
    - Read-only parameters (that were set by the server) cannot be deleted.
    - Here is an example invocation:
<pre>
MessageRef delPMsg = GetMessageFromPool(PR_COMMAND_REMOVEPARAMETERS);
delPMsg()->AddString(PR_NAME_KEYS, "SUBSCRIBE:/\\\*/\\\*/node\\\*");  // cancel one specific subscription
delPMsg()->AddString(PR_NAME_KEYS, "SUBSCRIBE:\*");  // cancel all my subscriptions!
myMessageTransceiverThread.SendMessageToSessions(delPMsg);
</pre>

* `PR_COMMAND_BATCH` - *Any sub-Messages in the `PR_NAME_KEYS` Message-field will be handled in-order, as if they had been sent separately*
    - This is particularly useful for ensuring atomicity -- i.e. if you have several Messages to send to the server and you want to guarantee that they all get executed at the same time (with no other operations allowed to happen in between)
<pre>
MessageRef batchMsg = GetMessageFromPool(PR_COMMAND_BATCH);
batchMsg()->AddMessage(PR_NAME_KEYS, GetMessageFromPool(PR_COMMAND_SETPARAMETERS));
batchMsg()->AddMessage(PR_NAME_KEYS, GetMessageFromPool(PR_COMMAND_SETDATA));
batchMsg()->AddMessage(PR_NAME_KEYS, GetMessageFromPool(PR_COMMAND_GETPARAMETERS));
myMessageTransceiverThread.SendMessageToSessions(batchMsg);
</pre>
* `PR_COMMAND_INSERTORDEREDDATA` - *Insert nodes underneath a node, as an ordered list*
    - This is a specialized form of the `PR_COMMAND_SETDATA` command
    - It is useful for adding a [DataNode](https://public.msli.com/lcs/muscle/html/classmuscle_1_1DataNode.html) to its parent in such a way that it gets added to the parent node's indexed-nodes index.
    - If you don't care about maintaining a well-defined ordering of the child nodes relative to each other, then you don't need this command.
    - Here is an example invocation:
<pre>
MessageRef insertMsg = GetMessageFromPool(PR_COMMAND_INSERTORDEREDDATA);
insertMsg()->AddString(PR_NAME_KEYS, "blah");  // path of an already-existing node
insertMsg()->AddMessage("I5", GetMessageFromPool(1234));  // add before existing node I5
insertMsg()->AddMessage("I8", GetMessageFromPool(1234));  // add before existing node I8
insertMsg()->AddMessage("xxx", GetMessageFromPool(1234)); // append node (assuming node "xxx" doesn't exist)
myMessageTransceiverThread.SendMessageToSessions(insertMsg);
</pre>
* `PR_COMMAND_REORDERDATA` - *Moves one or more entries in an indexed-node's node-index to a different spot in its node-index*
    - This is useful only if you've previously been sending `PR_COMMAND_INSERTORDEREDDATA` Message to order child-nodes within their parent.
    - If you now want to change the ordering of the child nodes relative to each other, this is more efficient then deleting a child-node and then re-uploading it (especially if the child-node's [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) payload is large)
    - Here is an example invocation:
<pre>
MessageRef reorderMsg = GetMessageFromPool(PR_COMMAND_REORDERDATA);
reorderMsg()->AddString("blah/I8", "I5");  // move blah's child "I8" to before "I5"
reorderMsg()->AddString("blah/I9", "append");  // move blah's child "I9" to the end of the index
myMessageTransceiverThread.SendMessageToSessions(reorderMsg);
</pre>
* `PR_COMMAND_GETDATATREES` - *Returns an entire subtree of data as a single `PR_RESULT_DATATREES` Message*
    - The main difference between this an `PR_COMMAND_GETDATA` is that this form allows you to request an entire subtree to be sent back as a single [Message](https://public.msli.com/lcs/muscle/html/classmuscle_1_1Message.html) (no matter how deep the subtree is -- `PR_COMMAND_GETDATA` forces you to download only a finite number of levels of subtree)
    - Add a String in the `PR_NAME_REQUEST_ID` fieldname to label your request (reply Message will contain this value)
    - You can optionally limit the depth of nodes in the reply Message by specifying a `PR_NAME_MAXDEPTH` int32 field
    - Here is an example invocation:
<pre>
MessageRef reorderMsg = GetMessageFromPool(PR_COMMAND_GETDATATREES);
reorderMsg()->AddString(PR_NAME_TREE_REQUEST_ID, "my_request");
reorderMsg()->AddString(PR_NAME_KEYS, "subfolder");   // path of subtree to start at
reorderMsg()->AddInt32(PR_NAME_MAXDEPTH, 5);  // no more than 5 levels deep please
myMessageTransceiverThread.SendMessageToSessions(reorderMsg);
</pre>

Quick links to source code of relevant MUSCLE-by-example programs:

* [reflector/example_4_smart_server.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_4_smart_server.cpp)
* [reflector/example_5_smart_client.cpp](https://public.msli.com/lcs/muscle/muscle/html/muscle-by-example/examples/reflector/example_5_smart_client.cpp)
* [test/portablereflectclient.cpp](https://public.msli.com/lcs/muscle/muscle/test/portablereflectclient.cpp)
* [test/chatclient.cpp](https://public.msli.com/lcs/muscle/muscle/test/chatclient.cpp)
* [qtsupport/qt_example/qt_example.cpp](https://public.msli.com/lcs/muscle/muscle/qtsupport/qt_example/qt_example.cpp)
