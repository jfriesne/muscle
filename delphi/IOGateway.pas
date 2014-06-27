{-----------------------------------------------------------------------------
 Unit Name: IOGateway
 Author:    Matthew Emson
 Date:      02-Jul-2005
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



unit IOGateway;

interface

uses
  classes, sysutils, Message;


type
  IMessageIOGateway = interface ['{95FFB6A4-D937-4C06-861D-CF50F22C5E57}']
    function unflattenMessage(datain: TStream): IPortableMessage;
    procedure flattenMessage(dataout: TStream; const msg: IPortableMessage);
  end;

  TMessageIOGateway = class(TInterfacedObject, IMessageIOGateway)
  public
    function unflattenMessage(datain: TStream): IPortableMessage;
    procedure flattenMessage(dataout: TStream; const msg: IPortableMessage);
  end;

const
  MESSAGE_HEADER_SIZE = 8;

  MUSCLE_MESSAGE_ENCODING_DEFAULT = 1164862256; // 'Enc0'  ... our default (plain-vanilla) encoding

  // zlib encoding levels
  MUSCLE_MESSAGE_ENCODING_ZLIB_1 = 1164862257;
  MUSCLE_MESSAGE_ENCODING_ZLIB_2 = 1164862258;
  MUSCLE_MESSAGE_ENCODING_ZLIB_3 = 1164862259;
  MUSCLE_MESSAGE_ENCODING_ZLIB_4 = 1164862260;
  MUSCLE_MESSAGE_ENCODING_ZLIB_5 = 1164862261;
  MUSCLE_MESSAGE_ENCODING_ZLIB_6 = 1164862262;
  MUSCLE_MESSAGE_ENCODING_ZLIB_7 = 1164862263;
  MUSCLE_MESSAGE_ENCODING_ZLIB_8 = 1164862264;
  MUSCLE_MESSAGE_ENCODING_ZLIB_9 = 1164862265;
  MUSCLE_MESSAGE_ENCODING_END_MARKER = 1164862266;

  ZLIB_CODEC_HEADER_INDEPENDENT = 2053925219;
  ZLIB_CODEC_HEADER_DEPENDENT = 2053925218;

implementation

uses
  winsock;



{ TMessageIOGateway }

// ** Converts the given Message into bytes and sends it out the stream.
//  * @param out the data stream to send the converted bytes to.
//  * @param message the Message to convert.
//  * @throws IOException if there is an error writing to the stream.
//  *
procedure TMessageIOGateway.flattenMessage(dataout: TStream; const msg: IPortableMessage);
var
  t: cardinal;
begin
  if (msg = nil) then exit;
  
  t := msg.flattenedSize;
  dataout.write(t, sizeof(cardinal));
  t :=  MUSCLE_MESSAGE_ENCODING_DEFAULT;
  dataout.write(t, sizeof(cardinal));
  msg.flatten(dataout);
end;


// ** Reads from the input stream until a Message can be assembled and returned.
//  * @param in The input stream to read from.
//  * @throws IOException if there is an error reading from the stream.
//  * @throws UnflattenFormatException if there is an error parsing the data in the stream.
//  * @return The next assembled Message.
//  *
function TMessageIOGateway.unflattenMessage(datain: TStream): IPortableMessage;
var
  numBytes, enc: cardinal;

begin
  datain.read(numBytes, sizeof(cardinal));
  datain.read(enc, sizeof(cardinal));

  if not(enc = MUSCLE_MESSAGE_ENCODING_DEFAULT) then
    raise Exception.Create('Failed to unflatten Message header!');

   result := TPortableMessage.Create;
   try
     result.unflatten(datain, numBytes);
   except
     result := nil;
   end;
end;

end.
