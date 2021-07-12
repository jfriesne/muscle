{-----------------------------------------------------------------------------
 Unit Name: StorageReflectConstants
 Author:    matte
 Date:      05-Jul-2005
 Purpose:
 History:
-----------------------------------------------------------------------------}

// This file is based on MUSCLE, Copyright 2007 Meyer Sound Laboratories Inc.
// See the LICENSE.txt file included with the MUSCLE source for details.
//
// Copyright (c) 2005, Matthew Emson
// All rights reserved.
//
// Redistribution and use in source and binary forms,
// with or without modification, are permitted provided
// that the following conditions are met:
//
//    * Redistributions of source code must retain the
//      above copyright notice, this list of conditions
//      and the following disclaimer.
//    * Redistributions in binary form must reproduce
//      the above copyright notice, this list of
//      conditions and the following disclaimer in the
//      documentation and/or other materials provided
//      with the distribution.
//    * Neither the name of "Matthew Emson", current or
//      past employer of "Matthew Emson" nor the names
//      of any contributors may be used to endorse or
//      promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
// AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
// TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.



unit StorageReflectConstants;

interface

const
    BEGIN_PR_COMMANDS            = 558916400;
    
    /// Adds/replaces the given fields in the parameters table 
    PR_COMMAND_SETPARAMETERS     = 558916401; 
    
    /// Returns the current parameter set to the client
    PR_COMMAND_GETPARAMETERS     = 558916402; 
    
    /// deletes the parameters specified in PR_NAME_KEYS
    PR_COMMAND_REMOVEPARAMETERS  = 558916403; 
    
    /// Adds/replaces the given message in the data table
    PR_COMMAND_SETDATA           = 558916404; 
    
    /// Retrieves the given message(s) in the data table
    PR_COMMAND_GETDATA           = 558916405; 
    
    /// Removes the gives message(s) from the data table
    PR_COMMAND_REMOVEDATA        = 558916406; 
    
    /// Removes data from outgoing result messages
    PR_COMMAND_JETTISONRESULTS   = 558916407; 
    
    /// Insert nodes underneath a node, as an ordered list
    PR_COMMAND_INSERTORDEREDDATA = 558916408; 
    
    /// Echo this message back to the sending client
    PR_COMMAND_PING              = 558916409; 
    
    /// Kick matching clients off the server (Requires privilege)
    PR_COMMAND_KICK              = 558916410; 
    
    /// Add ban patterns to the server's ban list (Requires privilege)
    PR_COMMAND_ADDBANS           = 558916411; 
    
    /// Remove ban patterns from the server's ban list (Requires privilege)
    PR_COMMAND_REMOVEBANS        = 558916412; 
    
    /// Submessages under PR_NAME_KEYS are executed in order, as if they 
    /// came separately
    PR_COMMAND_BATCH             = 558916413; 
    
    /// Server will ignore this message
    PR_COMMAND_NOOP              = 558916414; 
    
    /// Move one or more intries in a node index to a different spot in 
    /// the index
    PR_COMMAND_REORDERDATA       = 558916415; 
    
    /// Add require patterns to the server's require list 
    /// (Requires ban privilege)
    PR_COMMAND_ADDREQUIRES       = 558916416; 
    
    /// Remove require patterns from the server's require list 
    /// (Requires ban privilege)
    PR_COMMAND_REMOVEREQUIRES    = 558916417; 
    
    /// Reserved for future expansion
    PR_COMMAND_RESERVED11        = 558916418; 
    
    /// Reserved for future expansion
    PR_COMMAND_RESERVED12        = 558916419; 
    
    /// Reserved for future expansion
    PR_COMMAND_RESERVED13        = 558916420; 
    
    /// Reserved for future expansion
    PR_COMMAND_RESERVED14        = 558916421; 
    
    /// Reserved for future expansion */
    PR_COMMAND_RESERVED15        = 558916422; 
    
    /// Reserved for future expansion */
    PR_COMMAND_RESERVED16        = 558916423; 
    
    /// Reserved for future expansion */
    PR_COMMAND_RESERVED17        = 558916424; 
    
    /// Reserved for future expansion */
    PR_COMMAND_RESERVED18        = 558916425; 
    
    /// Reserved for future expansion */
    PR_COMMAND_RESERVED19        = 558916426; 
    
    /// Reserved for future expansion */
    PR_COMMAND_RESERVED20        = 558916427; 
    
    /// Reserved for future expansion */
    PR_COMMAND_RESERVED21        = 558916428; 
    
    /// Reserved for future expansion */
    PR_COMMAND_RESERVED22        = 558916429; 
    
    /// Reserved for future expansion */
    PR_COMMAND_RESERVED23        = 558916430; 
    
    /// Reserved for future expansion */
    PR_COMMAND_RESERVED24        = 558916431; 
    
    /// Reserved for future expansion */
    PR_COMMAND_RESERVED25        = 558916432; 
    
    /// Dummy value to indicate the end of the reserved command range
    END_PR_COMMANDS              = 558916433;
    
    /// Marks beginning of range of 'what' codes that may be generated 
    /// by the StorageReflectSession and sent back to the client
    BEGIN_PR_RESULTS             = 558920240; 
    
    /// Sent to client in response to PR_COMMAND_GETPARAMETERS
    PR_RESULT_PARAMETERS         = 558920241; 
    
    /// Sent to client in response to PR_COMMAND_GETDATA, or subscriptions
    PR_RESULT_DATAITEMS          = 558920242; 
   
    /// Sent to client to tell him that we don't know how to process 
    /// his request message
    PR_RESULT_ERRORUNIMPLEMENTED = 558920243; 
    
    /// Notification that an entry has been inserted into an ordered index
    PR_RESULT_INDEXUPDATED       = 558920244; 
    
    /// Response from a PR_COMMAND_PING message */
    PR_RESULT_PONG               = 558920245; 
    
    /// Your client isn't allowed to do something it tried to do
    PR_RESULT_ERRORACCESSDENIED  = 558920246; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED4          = 558920247; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED5          = 558920248; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED6          = 558920249; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED7          = 558920250; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED8          = 558920251; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED9          = 558920252; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED10         = 558920253; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED11         = 558920254; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED12         = 558920255; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED13         = 558920256; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED14         = 558920257; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED15         = 558920258; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED16         = 558920259; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED17         = 558920260; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED18         = 558920261; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED19         = 558920262; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED20         = 558920263; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED21         = 558920264; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED22         = 558920265; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED23         = 558920266; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED24         = 558920267; 
    
    /// Reserved for future expansion
    PR_RESULT_RESERVED25         = 558920268; 
    
    /// Reserved for future expansion
    END_PR_RESULTS               = 558920269; 
    
    /// Privilege bit, indicates that the client is allowed to kick 
    /// other clients off the MUSCLE server
    PR_PRIVILEGE_KICK            = 0;
    
    /// Privilege bit, indicates that the client is allowed to ban 
    /// other clients from the MUSCLE server
    PR_PRIVILEGE_ADDBANS         = 1;
    
    /// Privilege bit, indicates that the client is allowed to unban 
    /// other clients from the MUSCLE server
    PR_PRIVILEGE_REMOVEBANS      = 2;
    
    /// Number of defined privilege bits
    PR_NUM_PRIVILEGES            = 3;
    
    /// Op-code that indicates that an entry was inserted at the 
    /// given slot index, with the given ID
    INDEX_OP_ENTRYINSERTED = 'i';
    
    /// Op-code that indicates that an entry was removed from the 
    /// given slot index, had the given ID
    INDEX_OP_ENTRYREMOVED  = 'r';   
    
    /// Op-code that indicates that the index was cleared */
    INDEX_OP_CLEARED       = 'c';
    
    /// Field name for key-strings (often node paths or regex expressions)
    PR_NAME_KEYS                      = '!SnKy';
    
    /// Field name to contains node path strings of removed data items
    PR_NAME_REMOVED_DATAITEMS         = '!SnRd'; 
    
    /// Field name (any type):  If present in a PR_COMMAND_SETPARAMETERS 
    /// message, disables inital-value-send from new subscriptions
    PR_NAME_SUBSCRIBE_QUIETLY         = '!SnQs'; 
    
    /// Field name (any type):  If present in a PR_COMMAND_SETDATA message, 
    /// subscribers won't be notified about the change.
    PR_NAME_SET_QUIETLY               = '!SnQ2'; 
    
    /// Field name (any type):  If present in a PR_COMMAND_REMOVEDATA 
    /// message, subscribers won't be notified about the change.
    PR_NAME_REMOVE_QUIETLY            = '!SnQ3'; 

    // Parameter name holding int32 of MUSCLE_MESSAGE_ENCODING_* used to 
    // send to client
    PR_NAME_REPLY_ENCODING            = '!Enc';
    
    /// Field name (any type):  If set as parameter, include ourself 
    /// in wildcard matches
    PR_NAME_REFLECT_TO_SELF           = '!Self'; 
    
    /// Field name (any type):  If set as a parameter, disable all 
    ///   subscription update messaging.
    PR_NAME_DISABLE_SUBSCRIPTIONS     = '!Dsub'; 
    
    /// Field name of int parameter; sets max # of items per 
    /// PR_RESULT_DATAITEMS message
    PR_NAME_MAX_UPDATE_MESSAGE_ITEMS  = '!MxUp'; 
    
    /// Field name of string returned in parameter set; 
    /// contains this session's /host/sessionID path
    PR_NAME_SESSION_ROOT              = '!Root'; 
    
    /// Field name for Message: In PR_RESULT_ERROR_* messages, 
    /// holds the client's message that failed to execute.
    PR_NAME_REJECTED_MESSAGE          = '!Rjct'; 
    
    /// Field name of int32 bitchord of client's PR_PRIVILEGE_* bits.
    PR_NAME_PRIVILEGE_BITS            = '!Priv'; 
    
    /// Field name of int64 indicating how many more bytes are available 
    /// for MUSCLE server to use
    PR_NAME_SERVER_MEM_AVAILABLE      = '!Mav';
    
    /// Field name of int64 indicating how many bytes 
    /// the MUSCLE server currently has allocated */
    PR_NAME_SERVER_MEM_USED           = '!Mus';   
    
    /// Field name of int64 indicating how the maximum number of bytes 
    /// the MUSCLE server may have allocated at once.
    PR_NAME_SERVER_MEM_MAX            = '!Mmx';
    
    /// Field name of string indicating version of MUSCLE that the server 
    /// was compiled from
    PR_NAME_SERVER_VERSION            = '!Msv';
    
    /// Field name of int64 indicating how many microseconds have 
    /// elapsed since the server was started.
    PR_NAME_SERVER_UPTIME             = '!Mup';
    
    /// Field name of int32 indicating how many database nodes may be 
    /// uploaded by this client (total).
    PR_NAME_MAX_NODES_PER_SESSION     = '!Mns';
    
    /// Field name of a string that the server will replace with the 
    /// session ID string of your session in any outgoing 
    /// client-to-client messages.
    PR_NAME_SESSION                   = 'session'; 
    
    /// this field name's submessage is the payload of the current node
    /// in the message created by 
    /// StorageReflectSession::SaveNodeTreeToMessage() 
    PR_NAME_NODEDATA                  = 'data';  
    
    /// this field name's submessage represents the children of the 
    /// current node (recursive) in the message created by 
    /// StorageReflectSession::SaveNodeTreeToMessage()
    PR_NAME_NODECHILDREN              = 'kids';
    
    /// this field name's submessage represents the index of the current 
    /// node in the message created by 
    /// StorageReflectSession::SaveNodeTreeToMessage()
    PR_NAME_NODEINDEX                 = 'index';

implementation

end.
