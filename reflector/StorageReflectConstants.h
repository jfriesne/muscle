/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleStorageReflectConstants_h
#define MuscleStorageReflectConstants_h

#include "support/MuscleSupport.h"

#ifdef __cplusplus
# include "support/BitChord.h"
#endif

#ifdef __cplusplus
namespace muscle {
#endif

/** 'What' codes understood to have special meaning by the StorageReflectSession class */
enum
{
   BEGIN_PR_COMMANDS = 558916400, /**< '!Pc0' */
   PR_COMMAND_SETPARAMETERS,      /**< Adds/replaces the given fields in the parameters table */
   PR_COMMAND_GETPARAMETERS,      /**< Returns the current parameter set to the client */
   PR_COMMAND_REMOVEPARAMETERS,   /**< deletes the parameters specified in PR_NAME_KEYS */
   PR_COMMAND_SETDATA,            /**< Adds/replaces the given message in the data table */
   PR_COMMAND_GETDATA,            /**< Retrieves the given message(s) in the data table */
   PR_COMMAND_REMOVEDATA,         /**< Removes the gives message(s) from the data table */
   PR_COMMAND_JETTISONRESULTS,    /**< Removes data from outgoing result messages */
   PR_COMMAND_INSERTORDEREDDATA,  /**< Insert nodes underneath a node, as an ordered list */
   PR_COMMAND_PING,               /**< Echo this message back to the sending client */
   PR_COMMAND_KICK,               /**< Kick matching clients off the server (Requires privilege) */
   PR_COMMAND_ADDBANS,            /**< Add ban patterns to the server's ban list (Requires privilege) */
   PR_COMMAND_REMOVEBANS,         /**< Remove ban patterns from the server's ban list (Requires privilege) */
   PR_COMMAND_BATCH,              /**< Messages in PR_NAME_KEYS are executed in order, as if they came separately */
   PR_COMMAND_NOOP,               /**< Does nothing at all */
   PR_COMMAND_REORDERDATA,        /**< Moves one or more entries in a node index to a different spot in the index */
   PR_COMMAND_ADDREQUIRES,        /**< Add require patterns to the server's require list (Requires ban privilege) */
   PR_COMMAND_REMOVEREQUIRES,     /**< Remove require patterns from the server's require list (Requires ban privilege) */
   PR_COMMAND_SETDATATREES,       /**< Sets an entire subtree of data from a single Message (Not implemented!) */
   PR_COMMAND_GETDATATREES,       /**< Returns an entire subtree of data as a single Message */
   PR_COMMAND_JETTISONDATATREES,  /**< Removes matching RESULT_DATATREES Messages from the outgoing queue */
   PR_COMMAND_RESERVED14,         /**< reserved for future expansion */
   PR_COMMAND_RESERVED15,         /**< reserved for future expansion */
   PR_COMMAND_RESERVED16,         /**< reserved for future expansion */
   PR_COMMAND_RESERVED17,         /**< reserved for future expansion */
   PR_COMMAND_RESERVED18,         /**< reserved for future expansion */
   PR_COMMAND_RESERVED19,         /**< reserved for future expansion */
   PR_COMMAND_RESERVED20,         /**< reserved for future expansion */
   PR_COMMAND_RESERVED21,         /**< reserved for future expansion */
   PR_COMMAND_RESERVED22,         /**< reserved for future expansion */
   PR_COMMAND_RESERVED23,         /**< reserved for future expansion */
   PR_COMMAND_RESERVED24,         /**< reserved for future expansion */
   PR_COMMAND_RESERVED25,         /**< reserved for future expansion */
   END_PR_COMMANDS                /**< guard value */
};

/** 'What' codes that may be generated by the StorageReflectSession and sent back to the client */
enum
{
   BEGIN_PR_RESULTS = 558920240, /**< '!Pr0' */
   PR_RESULT_PARAMETERS,         /**< Sent to client in response to PR_COMMAND_GETPARAMETERS */
   PR_RESULT_DATAITEMS,          /**< Sent to client in response to PR_COMMAND_GETDATA, or subscriptions */
   PR_RESULT_ERRORUNIMPLEMENTED, /**< Sent to client to tell him that we don't know how to process his request message */
   PR_RESULT_INDEXUPDATED,       /**< Notification that an entry has been inserted into an ordered index */
   PR_RESULT_PONG,               /**< Response from a PR_COMMAND_PING message */
   PR_RESULT_ERRORACCESSDENIED,  /**< Your client isn't allowed to do something it tried to do */
   PR_RESULT_DATATREES,          /**< Reply to a PR_COMMAND_GETDATATREES message */
   PR_RESULT_NOOP,               /**< Clients should ignore this message.  Servers can send this to check TCP connectivity. */
   PR_RESULT_RESERVED6,          /**< reserved for future expansion */
   PR_RESULT_RESERVED7,          /**< reserved for future expansion */
   PR_RESULT_RESERVED8,          /**< reserved for future expansion */
   PR_RESULT_RESERVED9,          /**< reserved for future expansion */
   PR_RESULT_RESERVED10,         /**< reserved for future expansion */
   PR_RESULT_RESERVED11,         /**< reserved for future expansion */
   PR_RESULT_RESERVED12,         /**< reserved for future expansion */
   PR_RESULT_RESERVED13,         /**< reserved for future expansion */
   PR_RESULT_RESERVED14,         /**< reserved for future expansion */
   PR_RESULT_RESERVED15,         /**< reserved for future expansion */
   PR_RESULT_RESERVED16,         /**< reserved for future expansion */
   PR_RESULT_RESERVED17,         /**< reserved for future expansion */
   PR_RESULT_RESERVED18,         /**< reserved for future expansion */
   PR_RESULT_RESERVED19,         /**< reserved for future expansion */
   PR_RESULT_RESERVED20,         /**< reserved for future expansion */
   PR_RESULT_RESERVED21,         /**< reserved for future expansion */
   PR_RESULT_RESERVED22,         /**< reserved for future expansion */
   PR_RESULT_RESERVED23,         /**< reserved for future expansion */
   PR_RESULT_RESERVED24,         /**< reserved for future expansion */
   PR_RESULT_RESERVED25,         /**< reserved for future expansion */
   END_PR_RESULTS                /**< guard value */
};

/** Privilege codes (if a client has these bits, he can do the associated actions) */
enum
{
   PR_PRIVILEGE_KICK = 0,   /**< indicates that the client can disconnect other clients from the server */
   PR_PRIVILEGE_ADDBANS,    /**< indicates that the client can add ban-patterns to the server */
   PR_PRIVILEGE_REMOVEBANS, /**< indicates that the client can remove ban-patterns from the server */
   PR_NUM_PRIVILEGES        /**< guard value */
};

/** Op-codes found as part of the strings sent in a PR_RESULT_INDEXUPDATED message */
enum
{
   INDEX_OP_ENTRYINSERTED = 'i', /**< Entry was inserted at the given slot index, with the given ID */
   INDEX_OP_ENTRYREMOVED  = 'r', /**< Entry was removed from the given slot index, had the given ID */
   INDEX_OP_CLEARED       = 'c'  /**< Index was cleared */
};

/// Flags that can be passed as a bit-chord to the StorageReflectSessoin::SetDataNode() method, or passed in the PR_NAME_FLAGS field of the PR_COMMAND_SETDATA Message
enum {
   SETDATANODE_FLAG_DONTCREATENODE = 0,  ///< Specify this bit if the SetDataNode() call should error out rather than creating a new DataNode.
   SETDATANODE_FLAG_DONTOVERWRITEDATA,   ///< Specify this bit if the SetDataNode() call should error out rather than overwriting the Message payload of an existing node.
   SETDATANODE_FLAG_QUIET,               ///< Specify this bit if the SetDataNode() call should suppress the node-updated-notifications that would otherwise be sent out to the node's subscribers
   SETDATANODE_FLAG_ADDTOINDEX,          ///< Specify this bit if you want the node to be added to the parent node's ordered-children index.
   SETDATANODE_FLAG_ENABLESUPERCEDE,     ///< Specify this bit if updates triggered by this action can cancel (and replace) any still-pending earlier updates generated by the same node
   NUM_SETDATANODE_FLAGS                 ///< Guard value
};
#ifdef __cplusplus
extern const char * _setDataNodeFlagLabels[];
DECLARE_LABELLED_BITCHORD_FLAGS_TYPE(SetDataNodeFlags, NUM_SETDATANODE_FLAGS, _setDataNodeFlagLabels);
#endif

// Recognized message field names
#define PR_NAME_KEYS                       "!SnKy"      /**< String:  One or more key-strings */
#define PR_NAME_FILTERS                    "!SnFl"      /**< Message: One or more archived QueryFilter objects */
#define PR_NAME_REMOVED_DATAITEMS          "!SnRd"      /**< String:  one or more key-strings of removed data items */
#define PR_NAME_SUBSCRIBE_QUIETLY          "!SnQs"      /**< Any type:  if present in a PR_COMMAND_SETPARAMETERS message, disables inital-value-send from new subscriptions */
#define PR_NAME_SET_QUIETLY                "!SnQ2"      /**< Any type:  if present in a PR_COMMAND_SETDATA message, then the message won't cause subscribers to be notified.  (Deprecated; prefer PR_NAME_FLAGS instead) */
#define PR_NAME_REMOVE_QUIETLY             "!SnQ3"      /**< Any type:  if present in a PR_COMMAND_REMOVEDATA message, then the message won't cause subscribers to be notified. */
#define PR_NAME_FLAGS                      "!SnQ4"      /**< Flattened SetDataNodeFlags (for PR_COMMAND_SETDATA) */
#define PR_NAME_REFLECT_TO_SELF            "!Self"      /**< If set as parameter, include ourself in wildcard matches */
#define PR_NAME_ROUTE_GATEWAY_TO_NEIGHBORS "!G2N"       /**< If set as parameter, session broadcasts unrecognized Messages to neighbors (set by default) */
#define PR_NAME_ROUTE_NEIGHBORS_TO_GATEWAY "!N2G"       /**< If set as parameter, session accepts unrecognized Messages from neighbors and sends them to gateway (set by default) */
#define PR_NAME_DISABLE_SUBSCRIPTIONS      "!Dsub"      /**< If set as a parameter, disable all subscription updates. */
#define PR_NAME_MAX_UPDATE_MESSAGE_ITEMS   "!MxUp"      /**< Int32 parameter; sets max # of items per PR_RESULT_DATAITEMS message */
#define PR_NAME_KEEPALIVE_INTERVAL_SECONDS "!Ksec"      /**< Int32 parameter; if set, the server will send a PR_RESULT_NOOP Message every (so many) seconds */
#define PR_NAME_SESSION_ROOT               "!Root"      /**< String returned in parameter set; contains this sessions /host/sessionID */
#define PR_NAME_REJECTED_MESSAGE           "!Rjct"      /**< Message: In PR_RESULT_ERROR_* messages, returns the client's message that failed to execute. */
#define PR_NAME_PRIVILEGE_BITS             "!Priv"      /**< int32 bit-chord of PR_PRIVILEGE_* bits. */
#define PR_NAME_SERVER_MEM_AVAILABLE       "!Mav"       /**< int64 indicating how many more bytes are available for MUSCLE server to use */
#define PR_NAME_SERVER_MEM_USED            "!Mus"       /**< int64 indicating how many bytes the MUSCLE server currently has allocated */
#define PR_NAME_SERVER_MEM_MAX             "!Mmx"       /**< uint64 indicating how the maximum number of bytes the MUSCLE server may have allocated at once. */
#define PR_NAME_SERVER_VERSION             "!Msv"       /**< String indicating version of MUSCLE that the server was compiled from */
#define PR_NAME_SERVER_UPTIME              "!Mup"       /**< uint64 indicating how many microseconds the server has been running for */
#define PR_NAME_SERVER_CURRENTTIMEUTC      "!Mct"       /**< uint64 indicating the server's current wall-clock (microseconds since 1970), in UTC format */
#define PR_NAME_SERVER_CURRENTTIMELOCAL    "!Mcl"       /**< uint64 indicating the server's current wall-clock (microseconds since 1970), in the server's local timezone format */
#define PR_NAME_SERVER_RUNTIME             "!Mrt"       /**< uint64 indicating the server's current run-time clock (microseconds) */
#define PR_NAME_SERVER_SESSION_ID          "!Ssi"       /**< uint64 that is unique to this particular instance of the server in this particular process */
#define PR_NAME_MAX_NODES_PER_SESSION      "!Mns"       /**< uint32 indicating the maximum number of nodes uploadable by a session */
#define PR_NAME_SESSION                    "session"    /**< this field will be replaced with the sender's session number for any client-to-client message (named "session" for BeShare backwards compatibility) */
#define PR_NAME_SUBSCRIBE_PREFIX           "SUBSCRIBE:" /**< Prefix for parameters that indicate a subscription request  */
#define PR_NAME_TREE_REQUEST_ID            "!TRid"      /**< Identifier field for associating PR_RESULT_DATATREES replies with PR_COMMAND_GETDATATREE commands */
#define PR_NAME_REPLY_ENCODING             "!Enc"       /**< Parameter name holding int32 of MUSCLE_MESSAGE_ENCODING_* used to send to client */
#define PR_NAME_MAXDEPTH                   "!MDep"      /**< If present as an int32 in PR_COMMAND_GETDATATREES, returned trees will be clipped to this maximum depth. (0==roots only) */

// Names in the output message generated by StorageReflectSession::SaveNodeTreeToMessage() */
#define PR_NAME_NODEDATA      "data"   /**< this submessage is the payload of the current node */
#define PR_NAME_NODECHILDREN  "kids"   /**< this submessage represents the children of the current node (recursive) */
#define PR_NAME_NODEINDEX     "index"  /**< this submessage represents index/ordering of the current node */

// This is a specialization of AbstractReflectSession that adds several
// useful capabilities to the Reflect Server.  Abilities include:
//   - Messages can specify (via wildcard path matching) which other
//     clients they should be reflected to.  If the message doesn't specify
//     a path, then the session's default path can be used.
//   - Clients can upload and store data on the server (in the server's RAM only).
//     This data will remain accessible to all sessions until the client disconnects.
//   - Clients can "subscribe" to server state information and be automatically
//     notified when the information has changed.
//
// CLIENT-TO-SERVER MESSAGE FORMAT SPECIFICATION:
//
// if 'what' is PR_COMMAND_SETPARAMETERS:
//    All fields of the message are placed into the session's parameter set.
//    Any fields in the previous parameters set with matching names are replaced.
//    Currently parsed parameter names are as follows:
//
//      "SUBSCRIBE:<path>" : Any parameter name that begins with the prefix SUBSCRIBE:
//                           is taken to represent a request to monitor the contents of
//                           all nodes whose pathnames match the path that follows.  So,
//                           for example, the parameter name
//
//                                SUBSCRIBE:/*/*/Joe
//
//                           Indicates a request to watch all nodes with paths that match
//                           the pattern matching expression /*/*/Joe.
//                           As of muscle 2.40, the value of this parameter may be a Message
//                           containing an archived QueryFilter object, in which case the
//                           subscription will use the archived QueryFilter to test matching
//                           nodes in addition to the path matching.  If the parameter value
//                           is not a Message object, no QueryFilter will be used.
//                           Thus, these parameters may be of any type.  Once a SUBSCRIBE
//                           parameter has been added, any data nodes that match the specified
//                           path will be returned immediately to the client in a PR_RESULT_DATAITEMS
//                           message.  Furthermore, any time these nodes are modified or deleted,
//                           or any time a new node is added that matches the path, another
//                           PR_RESULT_DATAITEMS message will be sent to notify the client of
//                           the change.
//
//      PR_NAME_KEYS : If set, any non-"special" messages without a
//                     PR_NAME_KEYS field will be reflected to clients
//                     who match at least one of the set of key-paths
//                     listed here.  (Should be one or more string values)
//
//      PR_NAME_FILTERS : If the PR_NAME_KEYS parameter is set, this parameter
//                        may be set also, and the reflecting of non-special
//                        messages will be filtered using the QueryFilter specified in this parameter.
//
//      PR_NAME_REFLECT_TO_SELF : If set, wildcard matches can match the current session.
//                                Otherwise, wildcard matches with the current session will
//                                be suppressed (on the grounds that your client already knows
//                                the value of anything that it uploaded, and doesn't need to
//                                be reminded of it).  This field may be of any type, only
//                                its existence/non-existence is relevant.  This parameter is NOT set by default.
//
//      PR_NAME_ROUTE_GATEWAY_TO_NEIGHBORS : If set, then any unrecognized Message received by the current
//                                           session will be broadcast to all other neighboring sessions.
//                                           This parameter is set by default.
//
//      PR_NAME_ROUTE_NEIGHBORS_TO_GATEWAY : If set, then any unrecognized Message received from a neighboring
//                                           session will be sent out to the current session's gateay (and thus
//                                           to the client).  This parameter is set by default.
//
//      PR_NAME_REPLY_ENCODING : If set, this int32 specifies the MUSCLE_MESSAGE_ENCODING_*
//                               value to be used by the session when sending data back to the client.
//                               If unset, the default value (MUSCLE_MESSAGE_ENCODING_DEFAULT) is used.
//                               Setting this parameter is useful if you want the server to compress
//                               the data it sends back to your client.
//
//
// if 'what' is PR_COMMAND_GETPARAMETERS:
//    Causes a PR_RESULT_PARAMETERS message to be returned to the client.  The returned message
//    contains the entire current parameter set.
//
// if 'what' is PR_COMMAND_REMOVEPARAMETERS:
//    The session looks for PR_NAME_KEYS string entrys.  For each string found
//    under this entry, any matching field in the parameters message are deleted.
//    Wildcards are permitted in these strings.  (So eg "*" would remove ALL parameters)
//
// if 'what' is PR_COMMAND_SETDATA:
//    Scans the message for all fields of type message.  Each message field
//    should contain only one message.  The field's name is parsed as a local
//    key-path of the data item (eg "myData", or "imageInfo/colors/red").
//    Each contained message will be stored in the local session's data tree under
//    that key path.  (Note:  fields that start with a '/' are not allowed, and
//    will be ignored!)
//
// if 'what' is PR_COMMAND_REMOVEDATA:
//    Removes all data nodes that match the path(s) in the PR_NAME_KEYS string field.
//    Paths should be specified relative to this session's root node (ie they should
//    not start with a slash)  QueryFilter objects may be provided in the PR_NAME_FILTERS
//    Message field.  Any PR_NAME_KEYS string without a matching PR_NAME_FILTERS Message
//    will remove data based solely on path matching, without any QueryFilter object.
//
// if 'what' is PR_COMMAND_GETDATA:
//    The session looks for one or more strings in the PR_NAME_KEYS field.  Each
//    string represents a key-path indicating which information the client is
//    interested in retrieving.  If there is no leading slash, "/*/*/" will be
//    implicitely prepended.  Here are some valid example key-paths:
//         /*/*/color  (gets "color" from all hostnames, all session IDs)
//         /joe.blow.com/*/shape (gets "shape" from all sessions connected from joe.blow.com)
//         /joe.blow.com/19435935093/sound (gets "sound" from a single session)
//         /*/*/vehicleTypes/* (gets all vehicle types from all clients)
//         j* (equivalent to "/*/*/j*")
//         shape/* (equivalent to "/*/*/shape/*")
//    The union of all the sets of matching data nodes specified by these paths will be
//    added to one or more PR_RESULT_DATAITEMS message which is then passed back to the client.
//    Each matching message is added with its full path as a field name.
//    A PR_NAME_FILTERS field may be added to further limit the nodes matched
//    by the PR_NAME_KEYS field.
//
// if 'what' is PR_COMMAND_INSERTORDEREDDATA:
//    The session looks for one or more messages in the PR_NAME_KEYS field.  Each
//    string represents a wildpath, rooted at this session's node (read: no leading
//    slash should be present) that specifies zero or more data nodes to insert ordered/
//    indexed children under.  Each node in the union of these node sets will have new
//    ordered/indexed child nodes created underneath it.  The names of these new child
//    nodes will be chosen algorithmically by the server.  There will be one child node
//    created for each sub-message in this message.  Sub-messages may be added under any
//    field name; if the field name happens to be the name of a currently indexed child,
//    the new message node will be be inserted *before* the specified child in the index.
//    Otherwise, it will be appended to the end of the index.  Clients who have subscribed
//    to the specified nodes will see the updates to the index; clients who have subscribed
//    to the children will get updates of the actual data as well.
//    A PR_NAME_FILTERS field may be added to further limit the nodes matched
//    by the PR_NAME_KEYS field.
//
// if 'what' is PR_COMMAND_REORDERDATA:
//    The session looks for one or more strings in the message.  Each string field's name
//    will be used as a wild path, matching zero or more nodes in the node tree;  each
//    string field's value may be the name of a currently existing indexed child under the
//    matching node;  if so, the node(s) specified by the wild path will be reordered
//    in the index to appear just before the node specified in the string value.
//    If the string field's value is not the name of a child of the matching node, then the
//    nodes specified in the string field's name will be moved to the end of the index.
//    A PR_NAME_FILTERS field may be added to further limit the nodes matched
//    by the PR_NAME_KEYS field.
//
// if 'what' is PR_COMMAND_PING:
//    The session will change the message's 'what' code to PR_RESULT_PONG, and send
//    it right back to the client.  In this way, the client can find out (a) that
//    the server is still alive, (b) how long it takes the server to respond, and
//    (c) that any previously sent operations have finished.
//
// if 'what' is PR_COMMAND_KICK:
//    The server will look for one or more strings in the PR_NAME_KEYS field.  It will
//    do a search of the database for any nodes matching one or more of the node paths
//    specified by these strings, and will kick any session with matching nodes off
//    of the server.  Of course, this will only be done if the client who sent the
//    PR_COMMAND_KICK field has PR_PRIVILEGE_KICK access.
//
// if 'what' is PR_COMMAND_ADDBANS:
//    The server will look for one or more strings in the PR_NAME_KEYS field.  Any
//    strings that are found will be added to the server's "banned IP list", and
//    subsequent connection attempts from IP addresses matching any of these ban strings
//    will be denied.  Of course, this will only be done if the client who sent the
//    PR_COMMAND_ADDBANS field has PR_PRIVILEGE_ADDBANS access.
//
// if 'what' is PR_COMMAND_REMOVEBANS:
//    The server will look for one or more strings in the PR_NAME_KEYS field.  Any
//    strings that are found will be used a pattern-matching strings against the
//    current set of "ban" patterns.  Any "ban" patterns that are matched by any
//    of the PR_NAME_KEYS strings will be removed from the "banned IP patterns" set,
//    so that IP addresses who matched those patterns will be able to connect to
//    the server again.  Of course, this will only be done if the client who sent the
//    PR_COMMAND_REMOVEBANS field has PR_PRIVILEGE_REMOVEBANS access.
//
// if 'what' is PR_COMMAND_RESERVED_*:
//    The server will change the 'what' code of your message to PR_RESULT_UNIMPLEMENTED,
//    and send it back to your client.
//
// if 'what' is PR_RESULT_*:
//    The message will be silently dropped.  You are not allowed to send PR_RESULT_(*)
//    messages to the server, and should be very ashamed of yourself for even thinking
//    about it.
//
// All other 'what' codes
//    Messages with other 'what' codes are simply reflected to other clients verbatim.
//    If a PR_NAME_KEYS string field is found in the message, then it will be parsed as a
//    set of key-paths, and only other clients who can match at least one of these key-paths
//    will receive the message.  If no PR_NAME_KEYS field is found, then the parameter
//    named PR_NAME_KEYS will be used as a default value.  If that parameter isn't
//    found, then the message will be reflected to all clients (except this one).
//
// SERVER-TO-CLIENT MESSAGE FORMAT SPECIFICATION:
//
// if 'what' is PR_RESULT_PARAMETERS:
//    The message contains the complete parameter set that is currently associated with
//    this client on the server.  Parameters may have any field name, and be of any type.
//    Certain parameter names are recognized by the StorageReflectSession as having special
//    meaning; see the documentation on PR_COMMAND_SETPARAMETERS for information about these.
//
// if 'what' is PR_RESULT_DATAITEMS:
//    The message contains information about data that is stored on the server.  All stored
//    data is stored in the form of Messages.  Thus, all data in this message will be
//    in the form of a message field, with the field's name being the fully qualified path
//    of the node it represents (eg "/my.computer.com/5/MyNodeName") and the value being
//    the stored data itself.  Occasionally it is necessary to inform the client that a data
//    node has been deleted; this is done by adding the deceased node's path name as a string
//    to the PR_NAME_REMOVED_DATAITEM field.  If multiple nodes were removed, there may be
//    more than one string present in the PR_NAME_REMOVED_DATAITEM field.
//
// if 'what' is PR_RESULT_INDEXUPDATED:
//    The message contains information about index entries that were added (via PR_COMMAND_INSERTORDERREDDATA)
//    to a node that the client is subscribed to.  Each entry's field name is the fully qualified
//    path of a subscribed node, and the value(s) are strings of this format:  "%c%lu:%s", %c is
//    a single character that is one of the INDEX_OP_* values, the %lu is an index the item was added to
//    or removed from, and %s is the key string for the child in question.
//    Note that there may be more than one value per field!
//
// if 'what' is PR_RESULT_ERRORUNIMPLEMENTED:
//    Your message is being returned to you because it tried to use functionality that
//    hasn't been implemented on the server side.  This usually happens if you are trying
//    to use a new MUSCLE feature with an old MUSCLE server.
//
// if 'what' is PR_RESULT_PONG:
//    Your PR_COMMAND_PING message has been returned to you as a PR_RESULT_PONG message.
//
// if 'what' is PR_RESULT_ERRORACCESSDENIED:
//    You tried to do something that you don't have permission to do (such as kick, ban,
//    or unban another user).
//
// if 'what' is PR_RESULT_DATATREES:
//    This is the response to a PR_COMMAND_GETDATATREES message you sent earlier.  If
//    contains the contents of the subtree(s) you requested.
//
// if 'what' is PR_RESULT_NOOP:
//    This is a dummy Message, probably sent just to force the TCP layer to verify that
//    connectivity is still present.  Please ignore it.
//
// if 'what' is anything else:
//    This message was reflected to your client by a neighboring client session.  The content
//    of the message is not specified by the StorageReflectSession; it just passes any message
//    on verbatim.

#ifdef __cplusplus
} // end namespace muscle
#endif

#endif
