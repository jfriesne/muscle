{-----------------------------------------------------------------------------
 Unit Name: MessageTransceiver
 Author:    matte
 Date:      05-Jul-2005
 Purpose:   Simple non threaded message tranceiver class
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

unit MessageTransceiver;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ScktComp, IOGateway, Message, MessageQueue;

const
  MTT_EVENT_CONNECTED     = 0;
  MTT_EVENT_DISCONNECTED  = 1;

type
  //used internally to hold data.
  TMessageBuffer = record
    size: cardinal;
    pos: cardinal;
    readSize: boolean;
    readEnc: boolean;
    buffer: TMemoryStream;
  end;

  //the events.. currently the Message received is a little surplus to requirements because
  //messages are not queued when using it. In a future implementation this will be handled
  //bo another thread and will give a better overall effect.
  TMessageReceivedEvent = procedure (const msgRef: IPortableMessage) of object;
  THandleMessageEvent = procedure (msgRef: IPortableMessage; var handled: boolean) of object;

  //simple implementation in the shape of a component. Install and place on form at
  //design time or use it in code - choice is yours. Currently rampantly single threaded
  //and so not very efficient. I expect to have a "MessageTranceiverThread" that contains
  //this class at some point. That will then put the comms in a seperate thread than the
  //VCL, and will hopefully make life snappier.
  //NB. we're using Borland's Delphi 5+ socket implementation.. sorry. It's the lowest
  //    common denominator. At some point I will implement some "socketsource" style
  //    alternative. You'll then be able to use other brands of socket (Synapse, indy,
  //    FPiette's etc)
  //For now... Enjoy..
  TMessageTransceiver = class(TComponent)
  private
    _socket: TClientSocket;

    _InBuffer: TMessageBuffer;
    _OutBuffer: TMessageBuffer;
    _tmpbuffer: TMemoryStream;

    _InQueue: TMessageQueue;
    _OutQueue: TMessageQueue;

    _gateway: TMessageIOGateway;

    FHandleMessage: THandleMessageEvent;
    FMessageReceived: TMessageReceivedEvent;
    function getConnected: boolean;
    function getHost: string;
    function getPort: integer;
    procedure setHost(const Value: string);
    procedure setPort(const Value: integer);

  protected
    procedure dowork; //does the writing
    procedure SocketRead(Sender: TObject; Socket: TCustomWinSocket);
    procedure SocketWrite(Sender: TObject; Socket: TCustomWinSocket); //does this even work??
    procedure SocketConnect(Sender: TObject; Socket: TCustomWinSocket);
    procedure SocketDisconnect(Sender: TObject; Socket: TCustomWinSocket);

  public
    constructor Create(acomponent: TComponent); override;
    destructor  Destroy; override;  //NB. we currently DO NOT flush the queues, so any messages in transit or
                                    //    on the queue need to be processed *before* destruction or they will
                                    //    be forever lost.

    //call this to post essages to the server for despatch to clients
    function SendMessageToSessions(const msgRef: IPortableMessage): boolean;

    //call this to _Manually_ get messages from the queue. To do this you must make sure
    //messages end up on the queue. Easiest way to do this is to ignore OnMessageReceived and
    //either ignore OnHandleMessage as well or implement it returning "handled = false" unless you
    //really have to do something else.
    function GetNextEvent(out evtCode: cardinal; {optional?} out msgCount: cardinal): IPortableMessage;  //returns events left on queue

    //socket stuff
    procedure connect(const ahost: string; const aport: integer); overload;
    procedure connect; overload;  //use this one if you've set the host and port at design time
                                  //NB. by default these will be '127.0.0.1' and '2960'

    procedure disconnect;

    property connected: boolean read getConnected; //reads the socket's connection status

    property socket: TClientSocket read _socket; //raw access to the socket. Runtime only.

  published
    property Host: string read getHost write setHost;
    property Port: integer read getPort write setPort;

    //use this event to "peek" at the message..
    property OnHandleMessage: THandleMessageEvent read FHandleMessage write FHandleMessage;

    //use this event to grab the message immediately (after potential "peek")
    property OnMessageReceived: TMessageReceivedEvent read FMessageReceived write FMessageReceived;

  end;

procedure Register;

implementation

procedure Register;
begin
  RegisterComponents('MUSCLE', [TMessageTransceiver]);
end;

{ TMessageTransceiver }

{-----------------------------------------------------------------------------}
constructor TMessageTransceiver.Create;
begin
  inherited;
  _socket := TClientSocket.Create(nil);
  _socket.OnRead := SocketRead;
  _socket.OnWrite := SocketWrite; //I question whether this works?!
  _socket.OnConnect := SocketConnect;
  _socket.OnDisconnect := SocketDisconnect;
  _socket.ClientType := ctNonBlocking;

  //defaults... streaming *may* override these.
  _socket.port := 2960;
  _socket.host := '127.0.0.1';

  _InBuffer.buffer := TMemoryStream.Create;
  _OutBuffer.buffer := TMemoryStream.Create;

  _InQueue := TMessageQueue.Create;
  _OutQueue := TMessageQueue.Create;

  _gateway := TMessageIOGateway.Create;

  _tmpbuffer := TMemoryStream.Create;
end;

{-----------------------------------------------------------------------------}
destructor TMessageTransceiver.Destroy;
begin
  //note to self - probably need to fiddle with destruction order...

  _socket.free;

  _InBuffer.buffer.free;
  _OutBuffer.buffer.free;

  _InQueue.free;
  _OutQueue.free;

  _gateway.Free;

  _tmpbuffer.Free;

  inherited;
end;

{-----------------------------------------------------------------------------}
procedure TMessageTransceiver.connect(const ahost: string; const aport: integer);
begin
  _socket.host := ahost;
  _socket.port := aport;
  connect;
end;

{-----------------------------------------------------------------------------}
procedure TMessageTransceiver.connect;
begin
  if connected then
    raise Exception.Create('Socket is already connected!');

  _socket.active := true;
end;

{-----------------------------------------------------------------------------}
procedure TMessageTransceiver.disconnect;
begin
  _socket.Active := false;
end;

{-----------------------------------------------------------------------------}
procedure TMessageTransceiver.dowork;
var
  msg: IPortableMessage;
  c: cardinal;
  buffer: array[0..8191] of byte; //8KB buffer
  bytesread: cardinal;
begin
  repeat
    _OutBuffer.buffer.position := 0;
    _OutBuffer.buffer.size := 0;

    msg := _OutQueue.pop(c);

    if msg <> nil then
    begin
      _gateway.flattenMessage(_OutBuffer.buffer, msg);

      _OutBuffer.buffer.Position := 0; //rewind

      while (_OutBuffer.buffer.Position < _OutBuffer.buffer.Size) do
      begin
        bytesread := _OutBuffer.buffer.read(buffer, sizeof(buffer));
        _socket.Socket.SendBuf(buffer, bytesread);
      end;
    end;
  until c <= 0;
end;

{-----------------------------------------------------------------------------}
function TMessageTransceiver.getConnected: boolean;
begin
  result := _socket.Active;
end;

{-----------------------------------------------------------------------------}
function TMessageTransceiver.getHost: string;
begin
  result := _socket.Host;
end;

{-----------------------------------------------------------------------------}
function TMessageTransceiver.getPort: integer;
begin
  result := _socket.port;
end;

{-----------------------------------------------------------------------------}
procedure TMessageTransceiver.setHost(const Value: string);
begin
  _socket.Host := Value;
end;

{-----------------------------------------------------------------------------}
procedure TMessageTransceiver.setPort(const Value: integer);
begin
  _socket.port := Value;
end;

{-----------------------------------------------------------------------------}
function TMessageTransceiver.GetNextEvent(out evtCode, msgCount: cardinal): IPortableMessage;
begin
  result := _InQueue.pop(msgCount);
  if assigned(result) then
    evtCode := Result.what;
end;

{-----------------------------------------------------------------------------}
function TMessageTransceiver.SendMessageToSessions(const msgRef: IPortableMessage): boolean;
begin
  result := _OutQueue.push(msgref);
  dowork;
end;

{-----------------------------------------------------------------------------}
procedure TMessageTransceiver.SocketRead(Sender: TObject; Socket: TCustomWinSocket);
var
  t: cardinal;
  buffer: array[0..8191] of byte; //8KB buffer
  bytesread: cardinal;
  msg: IPortableMessage;
  handled: boolean;
begin
  if (_InBuffer.size = 0) then //result vars
  begin
    _InBuffer.readSize := false;
    _InBuffer.readEnc := false;
    _InBuffer.buffer.Position := 0;
    _InBuffer.buffer.Size := 0;
  end;

  //half arsed state machine:
  if not(_InBuffer.readSize) then
  begin
    bytesread := Socket.ReceiveBuf(_InBuffer.size, sizeof(cardinal));
    assert(bytesread = sizeof(cardinal));
    _InBuffer.buffer.write(_InBuffer.size, bytesread);
    _InBuffer.readSize := true;
  end
  else if not(_InBuffer.readEnc) then
  begin
    bytesread := Socket.ReceiveBuf(t, sizeof(cardinal));
    assert(bytesread = sizeof(cardinal));
    assert(t = MUSCLE_MESSAGE_ENCODING_DEFAULT);
    _InBuffer.buffer.write(t, bytesread);
    _InBuffer.readEnc := true;
  end
  else begin
    while (socket.ReceiveLength > 0) do
    begin
      if ((_InBuffer.size - _InBuffer.pos) < sizeof(buffer)) then
        t := _InBuffer.size - _InBuffer.pos
      else
        t := sizeof(buffer);

      bytesread := Socket.ReceiveBuf(buffer, sizeof(buffer));
      inc(_InBuffer.pos, bytesread);
      _InBuffer.buffer.write(buffer, bytesread);
    end;

    if (_InBuffer.pos >= _InBuffer.size) then
    begin
      _InBuffer.buffer.Position := 0;
      msg := _gateway.unflattenMessage(_InBuffer.buffer);

      handled := false;

      if assigned(FHandleMessage) then
        FHandleMessage(msg, handled);

      if handled then
      begin
        msg := nil; //free message
      end
      else begin
        if assigned(FMessageReceived) then
        begin
          FMessageReceived(msg);     //push message right away - dont cache
          msg := nil;                //this process owns the message and so we free it
        end
        else
          _InQueue.push(msg);        //put message on queue - we no longer "own" the message.
      end;

      _InBuffer.size := 0;
    end;
  end;
end;

{-----------------------------------------------------------------------------}
procedure TMessageTransceiver.SocketWrite(Sender: TObject; Socket: TCustomWinSocket);
begin
  dowork;
end;

{-----------------------------------------------------------------------------}
procedure TMessageTransceiver.SocketConnect(Sender: TObject; Socket: TCustomWinSocket);
var
  connectMsg: IPortableMessage;
  handled: boolean;
begin
  connectMsg := TPortableMessage.Create(MTT_EVENT_CONNECTED);

  //okay, so now we fake the message journey.. at some point I should just
  //make an "internal post message" type method, but it's 1am and I need sleep..

  handled := false;

  if assigned(FHandleMessage) then
    FHandleMessage(connectMsg, handled);

  if handled then
  begin
    connectMsg := nil; //free message
  end
  else begin
    if assigned(FMessageReceived) then
    begin
      FMessageReceived(connectMsg);     //push message right away - dont cache
      connectMsg := nil;                //this process owns the message and so we free it
    end
    else
      _InQueue.push(connectMsg);        //put message on queue - we no longer "own" the message.
  end;
end;

{-----------------------------------------------------------------------------}
procedure TMessageTransceiver.SocketDisconnect(Sender: TObject; Socket: TCustomWinSocket);
var
  disconnectMsg: IPortableMessage;
  handled: boolean;
begin
  disconnectMsg := TPortableMessage.Create(MTT_EVENT_DISCONNECTED);

  //okay, so now we fake the message journey.. at some point I should just
  //make an "internal post message" type method, but it's 1am and I need sleep..

  handled := false;

  if assigned(FHandleMessage) then
    FHandleMessage(disconnectMsg, handled);

  if handled then
  begin
    disconnectMsg := nil; //free message
  end
  else begin
    if assigned(FMessageReceived) then
    begin
      FMessageReceived(disconnectMsg);     //push message right away - dont cache
      disconnectMsg := nil;                //this process owns the message and so we free it
    end
    else
      _InQueue.push(disconnectMsg);        //put message on queue - we no longer "own" the message.
  end;
end;

end.
