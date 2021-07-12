{-----------------------------------------------------------------------------
 Unit Name: Unit1
 Author:    mathew emson
 Date:      04-Jul-2005
 Purpose:
 History:
-----------------------------------------------------------------------------}

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

unit Unit1;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,

  MessageTransceiver, Message, ExtCtrls, StdCtrls, ScktComp;

type
  TForm1 = class(TForm)
    Button1: TButton;
    Edit1: TEdit;
    Memo1: TMemo;
    MessageTransceiver1: TMessageTransceiver;
    Button2: TButton;
    procedure Button1Click(Sender: TObject);
    procedure MessageTransceiver1MessageReceived(msgRef: IPortableMessage);
    procedure Button2Click(Sender: TObject);
  private
    { Private declarations }
  public
    mt: TMessageTransceiver;
  end;

const
  TEXT_MESSAGE = 1886681204;

var
  Form1: TForm1;

implementation

{$R *.DFM}

procedure TForm1.Button1Click(Sender: TObject);
var
  msg: IPortableMessage;
begin
  msg := TPortableMessage.Create(TEXT_MESSAGE); //(PR_COMMAND_TEXT_STRINGS);
  msg.AddString('tl', edit1.text);
  MessageTransceiver1.SendMessageToSessions(msg);
end;

procedure TForm1.MessageTransceiver1MessageReceived(msgRef: IPortableMessage);
begin
  case msgRef.what of
    TEXT_MESSAGE:
      memo1.lines.add(msgRef.findstring('tl'));

    MTT_EVENT_CONNECTED:
      memo1.lines.add('Connected to server');

    MTT_EVENT_DISCONNECTED:
      memo1.lines.add('Server has disconnected');
      
  else
    memo1.lines.add(
      #13#10#13#10'----------'#13#10'<Unknown message>'#13#10#13#10+
      msgRef.toString + #13#10'----------');
  end;
end;

procedure TForm1.Button2Click(Sender: TObject);
begin
  MessageTransceiver1.connect;
end;

end.
