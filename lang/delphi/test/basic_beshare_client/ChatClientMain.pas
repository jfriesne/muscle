{-----------------------------------------------------------------------------
 Unit Name: ChatClientMain
 Author:    matthew emson
 Date:      06-Jul-2005
 Purpose:   A very basic BeShare client, based on the code for the PythonChat
            client.
 History:
-----------------------------------------------------------------------------}

//Portions based on BeShare and PythonChat, part of the MUSCLE distro.
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

unit ChatClientMain;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,

  MessageTransceiver, Message, ExtCtrls, StdCtrls, ScktComp, StorageReflectConstants;

type
  TDelphiShareClient = class(TForm)
    Memo1: TMemo;
    MessageTransceiver1: TMessageTransceiver;
    users: TListBox;
    Panel1: TPanel;
    Edit1: TEdit;
    Button1: TButton;
    Splitter1: TSplitter;
    Panel2: TPanel;
    Label1: TLabel;
    Button2: TButton;
    Label2: TLabel;
    Edit2: TEdit;
    Edit3: TEdit;
    procedure Button1Click(Sender: TObject);
    procedure MessageTransceiver1MessageReceived(
      const msgRef: IPortableMessage);
    procedure Edit1KeyUp(Sender: TObject; var Key: Word;
      Shift: TShiftState);
    procedure Button2Click(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure Edit2KeyUp(Sender: TObject; var Key: Word;
      Shift: TShiftState);

  private
    procedure sendChatMessage;
    procedure setUserName;

  public
    mt: TMessageTransceiver;
    username: string;
  end;

var
  DelphiShareClient: TDelphiShareClient;

implementation

{$R *.DFM}

const
  NET_CLIENT_NEW_CHAT_TEXT = 2;
  NET_CLIENT_PING          = 5;
  NET_CLIENT_PONG          = 6;

//# This method retrieves sessionID, e.g.
//# Given "/199.42.1.106/1308/beshare/name", it returns "1308"
function GetSessionID(x: string): string;
begin
   if (x[1] = '/') then
   begin
     delete(x, 1, 1);
     x := copy(x, pos('/', x) +1, length(x)); //x[x.find("/")+1:] //# remove leading slash, hostname and second slash
     x := copy(x, 1, pos('/', x) -1); //x = x[:x.find("/")]       //# remove everything after the session ID
     result := x;
   end
   else
      result := '';
end;

{-----------------------------------------------------------------------------
  Procedure: Button1Click
  Author:    mathew emson
  Date:      06-Jul-2005
  Purpose:
-----------------------------------------------------------------------------}
procedure TDelphiShareClient.Button1Click(Sender: TObject);
begin
  sendChatMessage;
end;

{-----------------------------------------------------------------------------
  Procedure: MessageTransceiver1MessageReceived
  Author:    mathew emson
  Date:      06-Jul-2005
  Purpose:
-----------------------------------------------------------------------------}
procedure TDelphiShareClient.MessageTransceiver1MessageReceived(
  const msgRef: IPortableMessage);

var
  subscribeMsg, dataMsg: IPortableMessage;
  sessionID, chatText, outBuff, user, str: string;
  i, count: integer;
begin
  case msgRef.what of
    MTT_EVENT_CONNECTED:
      begin
        sleep(500);
        memo1.lines.add('Connected...');
        setUserName;

        //# and subscribe to get updates regarding who else is on the server
        subscribeMsg := TPortableMessage.Create(StorageReflectConstants.PR_COMMAND_SETPARAMETERS);
        subscribeMsg.AddBoolean('SUBSCRIBE:beshare/name', true);
        MessageTransceiver1.SendMessageToSessions(subscribeMsg);
      end;

    MTT_EVENT_DISCONNECTED:
      memo1.lines.add('Disconnected...');

    NET_CLIENT_NEW_CHAT_TEXT:
      begin
        sessionID := msgRef.FindString('session');
        chatText := msgRef.FindString('text');

        outBuff := '';
        if (msgRef.countFields('private') > 0) then
          outBuff := '<PRIVATE> ';

        //do the "users search"
        i := users.Items.IndexOfObject(pointer(StrToInt(sessionID)));
        if (i > -1) then
          user := users.items[i]
        else
          user := '<Unknown>';

        if (comparetext(copy(chatText, 1, 3), '/me') = 0) then
          outbuff := outbuff + '<ACTION> ' + user + ' ' +copy(chatText, 3, length(chatText)) //this is okay... Delphi protects buffer overruns
        else
          outbuff := outbuff + '(' + sessionid + ') ' + user + ' ' + chatText;
        memo1.lines.add(outbuff);
      end;

    NET_CLIENT_PING:
      begin
        sessionID := msgRef.FindString('session');
        if (sessionID <> '') then
        begin
          msgref.what := NET_CLIENT_PONG;
          msgref.AddString(StorageReflectConstants.PR_NAME_KEYS, '/*/'+sessionID+'/beshare');
          msgref.AddString('session', 'blah');   //# server will set this
          msgref.AddString('version', 'delphiChat v1.0');
          MessageTransceiver1.SendMessageToSessions(msgref);
        end;
      end;

    NET_CLIENT_PONG: ;

    StorageReflectConstants.PR_RESULT_DATAITEMS:
      begin
        //# Username/session list updates!  Gotta scan it and see what it says

        //# First check for any node-removal notices
        //removedList = nextEvent.GetStrings(StorageReflectConstants.PR_NAME_REMOVED_DATAITEMS)

        count := msgRef.countFields(StorageReflectConstants.PR_NAME_REMOVED_DATAITEMS);
        //for str in removedList:
        while msgRef.findString(StorageReflectConstants.PR_NAME_REMOVED_DATAITEMS, count -1, str) do
        begin
          sessionID := GetSessionID(str);
          i := users.Items.IndexOfObject(pointer(StrToInt(sessionID)));
          if (i > -1) then
          begin
            chatText :=  'User ('+sessionID+') '+users.Items[i]+' has disconnected.';
            users.Items.Delete(i);
            memo1.lines.add(chatText);
          end;
          dec(count);
        end;



        //# Now check for any node-update notices
        for count := 0 to msgRef.FieldNames.Count -1 do
        begin
           str := msgRef.FieldNames.Strings[count];
           sessionID := GetSessionID(str);
           if (sessionID <> '') then
           begin
             datamsg := msgRef.findMessage(str);

             if (datamsg <> nil) then
             begin
               user := datamsg.FindString('name');
               if (user <> '') then
               begin
                 i := users.Items.IndexOfObject(pointer(StrToInt(sessionID)));
                 if (i > -1) then
                 begin
                   chatText := 'User ('+sessionID+') (a.k.a. '+users.Items.strings[i]+') is now known as ' + user;
                   users.Items.Strings[i] := user;
                 end
                 else begin
                   chatText := 'User ('+sessionID+') (a.k.a. '+user+') has connected to the server.';
                   users.Items.AddObject(user, pointer(StrToInt(sessionID)));
                 end;
                 memo1.lines.add(chatText);
               end;
             end;
           end;
        end;
      end;
  else
    // unhandled
  end;

  Application.ProcessMessages;
end;

{-----------------------------------------------------------------------------
  Procedure: sendChatMessage
  Author:    mathew emson
  Date:      06-Jul-2005
  Purpose:
-----------------------------------------------------------------------------}
procedure TDelphiShareClient.sendChatMessage;
var
  msg: IPortableMessage;
begin
  msg := TPortableMessage.Create(NET_CLIENT_NEW_CHAT_TEXT); //(PR_COMMAND_TEXT_STRINGS);
  msg.AddString(StorageReflectConstants.PR_NAME_KEYS, '/*/*/beshare');
  msg.AddString('session', 'blah');  //# server will set this for us

  if (comparetext(copy(Edit1.text, 1, 4), '/prv') = 0) then
  begin
    msg.AddString('text', copy(Edit1.text, 5, length(Edit1.text)));
    msg.AddBoolean('private', true);
  end
  else msg.AddString('text', Edit1.text);

  if (comparetext(copy(Edit1.text, 1, 4), '/prv') = 0) then
  begin
    //msg.AddBoolean('private', true);
    msg := nil;
    raise Exception.Create('Private messages are broken at the moment.. sorry.');
  end;

  MessageTransceiver1.SendMessageToSessions(msg);

  memo1.lines.add('<me> ' + Edit1.text);
  edit1.text := '';
  Application.ProcessMessages;
end;

{-----------------------------------------------------------------------------
  Procedure: Edit1KeyUp
  Author:    mathew emson
  Date:      06-Jul-2005
  Purpose:
-----------------------------------------------------------------------------}
procedure TDelphiShareClient.Edit1KeyUp(Sender: TObject; var Key: Word; Shift: TShiftState);
begin
  if (key = 13) then sendChatMessage;
end;

{-----------------------------------------------------------------------------
  Procedure: Button2Click
  Author:    mathew emson
  Date:      06-Jul-2005
  Purpose:
-----------------------------------------------------------------------------}
procedure TDelphiShareClient.Button2Click(Sender: TObject);
begin
  if not (MessageTransceiver1.connected) then
  begin
    MessageTransceiver1.connect(Edit3.text, 2960);
    Button2.caption := 'Disconnect';
  end
  else begin
    MessageTransceiver1.disconnect;
    Button2.caption := 'Connect';
  end;
end;

{-----------------------------------------------------------------------------
  Procedure: FormCreate
  Author:    mathew emson
  Date:      06-Jul-2005
  Purpose:
-----------------------------------------------------------------------------}
procedure TDelphiShareClient.FormCreate(Sender: TObject);
begin
  username := 'DelphiBinky';
  Edit2.Text := username;
end;

{-----------------------------------------------------------------------------
  Procedure: setUserName
  Author:    mathew emson
  Date:      06-Jul-2005
  Purpose:
-----------------------------------------------------------------------------}
procedure TDelphiShareClient.setUserName;
var
  nameMsg, uploadMsg: IPortableMessage;
begin
  //# Upload our user name so people know who we are
  nameMsg := TPortableMessage.Create;
  nameMsg.AddString('name', username);
  nameMsg.AddInteger('port', 0); //  # BeShare requires this, although we don't use it
  nameMsg.AddString('version_name', 'delphiChat');
  nameMsg.AddString('version_num', '1.0');
  uploadMsg := TPortableMessage.Create(StorageReflectConstants.PR_COMMAND_SETDATA);
  uploadMsg.AddMessage('beshare/name', nameMsg);
  MessageTransceiver1.SendMessageToSessions(uploadMsg);

  memo1.lines.add('You are now known as ' + username);
end;

{-----------------------------------------------------------------------------
  Procedure: Edit2KeyUp
  Author:    mathew emson
  Date:      06-Jul-2005
  Purpose:
-----------------------------------------------------------------------------}
procedure TDelphiShareClient.Edit2KeyUp(Sender: TObject; var Key: Word;
  Shift: TShiftState);
begin
  username := edit2.text;

  if (key = 13) then
    if MessageTransceiver1.connected then
    begin
      setUserName;
    end;
end;

end.
