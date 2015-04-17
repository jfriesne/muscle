{-----------------------------------------------------------------------------
 Unit Name:
 Author:    Matthew Emson
 Date:      05/06/2003
 Purpose:   Generic OO Message class for IPC.
 History:   1.0  Basic code implementation.
            1.1  09/06/2003,  M Emson
                 Messages can now contain messages. A reply could there-
                 fore contain the request.. Also allows for hierarchies
                 of data to be stored in message format. Also added a
                 DataCount and DataAt functionalits to genericly get at
                 the data in the message, you could search for data
                 using the FindTypeAtIndex to get the datatype and DataAt
                 to grab the item. I'm slowly (as nedded) adding more
                 specific routines to access types. StringAt and
                 MessageAt being the first two (as these are the
                 nastiest to access via DataAt.
            1.2  10/06/2003,  M Emson
                 Fixed bugs in the message flatteneing protocol.
            1.3  14/06/2003, M Emson
                 Made FindDataStream a function that returns boolean
                 This allows an easy way to detect no data
            1.4  ??/??/2003, M Emson   - fix memory leaks.

            1.5  04/11/2003,  M Emson
                 integrated port of Muscle message classes and added a
                 bridge conversion routine to BMessage classes.
            1.6  16/09/2004, M Emson    |
                 Removed all the legacy and additional cruft. Renamed
                 TNewBMessage to TBMessage. Moved a load of
                 ImucscleMessage routines into IBMessage to simplify
                 Access to them.
            1.7  04/07/2005, M Emson
                 Updated and cleaned for release to LCS for inclusion
                 with main MUSCLE distro. Licensed as modified BSD.

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




unit Message;

interface

uses
  Classes, sysutils, Contnrs,

  TypeConstants,
  Flattenable,
  Point,
  Rect;

type

  {.$define DONTUSESILENTEXCEPTIONS}//Use this to force exceptions to surface to the user...

{$IFDEF DONTUSESILENTEXCEPTIONS}
  EPortableMessageError = class(Exception); //bickety-bam stylee
{$ELSE DONTUSESILENTEXCEPTIONS}
  EPortableMessageError = class(EAbort); //dragon ninja stylee
{$ENDIF DONTUSESILENTEXCEPTIONS}

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //if you want to force a non-silent exception, use this as the base..
  EPortableMessageErrorAlt = class(Exception); //But use the code below rather than normal exception code...
  //otherwise you may miss an exception in normal use... This is where Borland having some kind of multiple inheritence or
  //flag within all exceptions for silent exceptions would have been cool... Dammit..
  //
  //use the following code when trapping for generic exceptions :-
  //  except
  //    on e: <MY_SEPECIFIC_EXCEPTION_CLASS> do begin //<--- all specific exceptions trapped first
  //      { ...handle }
  //    end;
  //    { ...etc }
  //    on e: Exception do begin             //<--- trap generic EPortableMessageError exceptions
  //      if (E is EPortableMessageError) then begin
  //        { ...trap specific error... }
  //      end
  //      else if (E is EPortableMessageErrorAlt) then begin //<--- trap generic EPortableMessageErrorAlt exceptions
  //        { ...trap specific error... }
  //      end
  //      else try   //<--- Trap all other exceptions....
  //        raise; //<--- re-raise the exception to make code treat like new exception
  //      except
  //        { ...other exception code... }
  //      end;
  //    end;
  //  end;
  //
  // This code could be worked into an *.inc file...

  EPortableMessageReadonlyError = class(EPortableMessageError);

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //for internal use.... once the hierarchy is in place, this will become private

  IFlattenable = interface ['{BEB9BD12-3F90-4AAA-9582-1D6E78631E79}']
    procedure Flatten(AOut: TStream);
    procedure Unflatten(ain: TStream; numBytes: integer);
    function flattenedSize: integer;
  end;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //Note to the cautious...
  //Remember, the more data you add to a message, the larger impact it has on the transport!!!
  //because messages are meant to be sent over tcp/ip, they are flattened before transport, and then
  //unflattened on the target machine. This has little impact for small messages.. but if you add streams
  //to the message, you *will* notice a really big speed degradation. Be cautious.. send a message that
  //warns of data pending and only *then* stream that data via another means... especially if it's larger
  //than 100k.
  //
  //At some point I plan a 'TBNetworkFile' that will address this issue.
  IPortableMessage = interface(IFlattenable)['{BBF40989-24B4-40B7-8C49-CB93A5B81A87}']

    //procedure AddVariant(const Name: string; const data: Variant; const datatype: TPortableMessageDataType); no longer supported
    procedure AddString(const Name: string; const data: string);
    procedure AddInteger(const Name: string; const data: integer);
    procedure AddCardinal(const Name: string; const data: cardinal);
    procedure AddBoolean(const Name: string; const data: boolean);
    procedure AddFloat(const Name: string; const data: single);
    procedure AddDouble(const Name: string; const data: double); //for TDateTime etc			//V1_4_4_x
    procedure AddDataStream(const Name: string; data: TStream);
    procedure AddMessage(const Name: string; data: IPortableMessage);

    function ReplaceString(const Name: string; const data: string): boolean;
    function ReplaceInteger(const Name: string; const data: integer): boolean;
    function ReplaceCardinal(const Name: string; const data: cardinal): boolean;
    function ReplaceBoolean(const Name: string; const data: boolean): boolean;
    function ReplaceFloat(const Name: string; const data: single): boolean;
    function ReplaceDouble(const Name: string; const data: double): boolean;				//V1_4_4_x
    function ReplaceMessage(const Name: string; data: IPortableMessage): boolean;

    //procedure FindVariant(const Name: string; var data: variant);
    function FindString(const Name: string): string; overload;
    function FindInteger(const Name: string): Integer; overload;
    function FindCardinal(const Name: string): Cardinal; overload;
    function FindFloat(const Name: string): single; overload;
    function FindBoolean(const Name: string): boolean; overload;
    function FindDouble(const Name: string): double; overload;						//V1_4_4_x
    function FindDataStream(const Name: string; data: TStream): boolean; overload;
    function FindDataStreamData(const Name: string; toStream: TStream; dataCount: integer): integer; overload; //1_4_2_2 - jase, your wish...
    function FindMessage(const Name: string): IPortableMessage; overload;

    function findString(const Name: string; var data: string): boolean; overload;
    function findInt8(const Name: string; var data: ShortInt): boolean; overload;
    function findInt16(const Name: string; var data: SmallInt): boolean; overload;
    function findInt32(const Name: string; var data: LongInt): boolean; overload;
    function findInt64(const Name: string; var data: Int64): boolean; overload;
    function findFloat(const Name: string; var data: single): boolean; overload;
    function findDouble(const Name: string; var data: double): boolean; overload;
    function findBool(const Name: string; var data: boolean): boolean; overload;
    function findMessage(const Name: string; var data: IPortableMessage): boolean; overload;

    function findString(const Name: string; index: integer; var data: string): boolean; overload;
    function findInt8(const Name: string; index: integer; var data: ShortInt): boolean; overload;
    function findInt16(const Name: string; index: integer; var data: SmallInt): boolean; overload;
    function findInt32(const Name: string; index: integer; var data: LongInt): boolean; overload;
    function findInt64(const Name: string; index: integer; var data: Int64): boolean; overload;
    function findFloat(const Name: string; index: integer; var data: single): boolean; overload;
    function findDouble(const Name: string; index: integer; var data: double): boolean; overload;
    function findBool(const Name: string; index: integer; var data: boolean): boolean; overload;
    function findMessage(const Name: string; index: integer; var data: IPortableMessage): boolean; overload;
    function findRect(const Name: string; index: integer; var data: TmuscleRect): boolean; overload;
    function findPoint(const Name: string; index: integer; var data: TmusclePoint): boolean; overload;

    function fieldNames: TStrings; //ME 06/07/2005 - wow... holy bring forward method at htis late stage...
    procedure Clear;

    function deleteField(const Name: string; index: integer = 0): boolean;

    function GetWhat: Cardinal;
    procedure SetWhat(const Value: Cardinal);
    property What: Cardinal read GetWhat write SetWhat;

    procedure copyFrom(const source: IPortableMessage);

    function toString: string; //DOH! should have done this a while ago...
    function countFields(fieldname: string; fieldType: cardinal = B_ANY_TYPE): integer;	overload;
    function countFields(fieldType: cardinal = B_ANY_TYPE): integer; overload;
  end;


  //This interface is kept because it still does a couple of things the IPortableMessage doesn't.
  ImuscleMessage = interface ['{85555CB0-7AA5-465C-9A87-A2C0AECDB951}']
    procedure addString(const Name: string; const data: string);
    procedure addInt8(const Name: string; const data: Shortint);
    procedure addInt16(const Name: string; const data: SmallInt);
    procedure addInt32(const Name: string; const data: LongInt);
    procedure addInt64(const Name: string; const data: int64);
    procedure addBool(const Name: string; const data: boolean);
    procedure addFloat(const Name: string; const data: single);
    procedure addDouble(const Name: string; const data: double);
    //procedure addMessage(const Name: string; data: TmuscleMessage);
    procedure addRect(const Name: string; data: TmuscleRect);
    procedure addPoint(const Name: string; data: TmusclePoint);

    function replaceString(const Name: string; const data: string): boolean;
    function replaceInt8(const Name: string; const data: ShortInt): boolean;
    function replaceInt16(const Name: string; const data: SmallInt): boolean;
    function replaceInt32(const Name: string; const data: LongInt): boolean;
    function replaceInt64(const Name: string; const data: Int64): boolean;
    function replaceBool(const Name: string; const data: boolean): boolean;
    function replaceFloat(const Name: string; const data: single): boolean;
    function replaceDouble(const Name: string; const data: double): boolean;
    //function replaceMessage(const Name: string; data: TmuscleMessage): boolean; overload;
    function replaceRect(const Name: string; data: TmuscleRect): boolean;
    function replacePoint(const Name: string; data: TmusclePoint): boolean;

    //function replaceMessage(const Name: string; index: integer; data: TmuscleMessage): boolean; overload;

    function findString(const Name: string; var data: string): boolean; overload;
    function findInt8(const Name: string; var data: ShortInt): boolean; overload;
    function findInt16(const Name: string; var data: SmallInt): boolean; overload;
    function findInt32(const Name: string; var data: LongInt): boolean; overload;
    function findInt64(const Name: string; var data: Int64): boolean; overload;
    function findFloat(const Name: string; var data: single): boolean; overload;
    function findDouble(const Name: string; var data: double): boolean; overload;
    function findBool(const Name: string; var data: boolean): boolean; overload;
    function findMessage(const Name: string; var data: IPortableMessage): boolean; overload;
    function findRect(const Name: string; var data: TmuscleRect): boolean; overload;
    function findPoint(const Name: string; var data: TmusclePoint): boolean; overload;

    function findString(const Name: string; index: integer; var data: string): boolean; overload;
    function findInt8(const Name: string; index: integer; var data: ShortInt): boolean; overload;
    function findInt16(const Name: string; index: integer; var data: SmallInt): boolean; overload;
    function findInt32(const Name: string; index: integer; var data: LongInt): boolean; overload;
    function findInt64(const Name: string; index: integer; var data: Int64): boolean; overload;
    function findFloat(const Name: string; index: integer; var data: single): boolean; overload;
    function findDouble(const Name: string; index: integer; var data: double): boolean; overload;
    function findBool(const Name: string; index: integer; var data: boolean): boolean; overload;
    function findMessage(const Name: string; index: integer; var data: IPortableMessage): boolean; overload;
    function findRect(const Name: string; index: integer; var data: TmuscleRect): boolean; overload;
    function findPoint(const Name: string; index: integer; var data: TmusclePoint): boolean; overload;

    function countFields(fieldname: string; fieldType: cardinal = B_ANY_TYPE): integer;	overload;				//V1_4_4_x
    function countFields(fieldType: cardinal = B_ANY_TYPE): integer; overload;

    procedure Clear;

    function deleteField(const Name: string; index: integer = 0): boolean;

    function toString: string;
  end;

  //------------------------------------------------------------------------------------------------------------------------------------------

  // muscle message is another message format (TM)

const
  OLDEST_SUPPORTED_PROTOCOL_VERSION = 1347235888; // 'PM00'
  CURRENT_PROTOCOL_VERSION = 1347235888; // 'PM00'

type
  TmuscleMessageField = class(TmuscleFlattenable)
  private
    FtypeCode: cardinal;
    FPayLoad: TList;

    procedure SetPayLoad(const Value: TList);

  public
    constructor Create; overload; virtual;
    constructor Create(AtypeCode: cardinal); overload; virtual;

    destructor Destroy; override;

    function Count: integer;
    function flattenedItemSize: integer; virtual;

    //from TmuscleFlattenable
    function isFixedSize: boolean; override;
    function typeCode: cardinal; override;

    function flattenedSize: integer; override;
    function cloneFlat: TmuscleFlattenable; override;
    procedure setEqualTo(setFromMe: TmuscleFlattenable); override; //throws ClassCastException;  ... assign???

    procedure flatten(AOut: TStream); override;
    function allowsTypeCode(code: integer): boolean; override;
    procedure unflatten(ain: TStream; numBytes: integer); override;

    //mainly for testing...
    function toString: string; override;

    property PayLoad: TList read FPayLoad write SetPayLoad;
  end;

  //------------------------------------------------------------------------------------------------------------------------------------------

  TmuscleMessage = class; //forward

  TmuscleCustomMessage = class(TmuscleFlattenable, ImuscleMessage)
  private
    Fwhat: cardinal;

    _fieldTable: TStringList;

    procedure Setwhat(const Value: cardinal);
    function Getwhat: cardinal;

    procedure ensureFieldTableAllocated;
    function getCreateOrReplaceField(fieldname: string; fieldType: cardinal): TmuscleMessageField;

  protected
    procedure addString(const Name: string; const data: string); virtual;
    procedure addInt8(const Name: string; const data: Shortint); virtual;
    procedure addInt16(const Name: string; const data: SmallInt); virtual;
    procedure addInt32(const Name: string; const data: LongInt); virtual;
    procedure addInt64(const Name: string; const data: int64); virtual;
    procedure addBool(const Name: string; const data: boolean); virtual;
    procedure addFloat(const Name: string; const data: single); virtual;
    procedure addDouble(const Name: string; const data: double); virtual;
    procedure addMessage(const Name: string; data: IPortableMessage); virtual;
    procedure addRect(const Name: string; data: TmuscleRect); virtual;
    procedure addPoint(const Name: string; data: TmusclePoint); virtual;

    function replaceString(const Name: string; const data: string): boolean; virtual;
    function replaceInt8(const Name: string; const data: ShortInt): boolean; virtual;
    function replaceInt16(const Name: string; const data: SmallInt): boolean; virtual;
    function replaceInt32(const Name: string; const data: LongInt): boolean; virtual;
    function replaceInt64(const Name: string; const data: Int64): boolean; virtual;
    function replaceBool(const Name: string; const data: boolean): boolean; virtual;
    function replaceFloat(const Name: string; const data: single): boolean; virtual;
    function replaceDouble(const Name: string; const data: double): boolean; virtual;
    function replaceMessage(const Name: string; data: IPortableMessage): boolean; overload; virtual;
    function replaceRect(const Name: string; data: TmuscleRect): boolean; virtual;
    function replacePoint(const Name: string; data: TmusclePoint): boolean; virtual;

    function replaceMessage(const Name: string; index: integer; data: IPortableMessage): boolean; overload; virtual;

    function findString(const Name: string; var data: string): boolean; overload; virtual;
    function findInt8(const Name: string; var data: ShortInt): boolean; overload; virtual;
    function findInt16(const Name: string; var data: SmallInt): boolean; overload; virtual;
    function findInt32(const Name: string; var data: LongInt): boolean; overload; virtual;
    function findInt64(const Name: string; var data: Int64): boolean; overload; virtual;
    function findFloat(const Name: string; var data: single): boolean; overload; virtual;
    function findDouble(const Name: string; var data: double): boolean; overload; virtual;
    function findBool(const Name: string; var data: boolean): boolean; overload; virtual;
    function findMessage(const Name: string; var data: IPortableMessage): boolean; overload; virtual;
    function findRect(const Name: string; var data: TmuscleRect): boolean; overload; virtual;
    function findPoint(const Name: string; var data: TmusclePoint): boolean; overload; virtual;

    function findString(const Name: string; index: integer; var data: string): boolean; overload; virtual;
    function findInt8(const Name: string; index: integer; var data: ShortInt): boolean; overload; virtual;
    function findInt16(const Name: string; index: integer; var data: SmallInt): boolean; overload; virtual;
    function findInt32(const Name: string; index: integer; var data: LongInt): boolean; overload; virtual;
    function findInt64(const Name: string; index: integer; var data: Int64): boolean; overload; virtual;
    function findFloat(const Name: string; index: integer; var data: single): boolean; overload; virtual;
    function findDouble(const Name: string; index: integer; var data: double): boolean; overload; virtual;
    function findBool(const Name: string; index: integer; var data: boolean): boolean; overload; virtual;
    function findMessage(const Name: string; index: integer; var data: IPortableMessage): boolean; overload; virtual;
    function findRect(const Name: string; index: integer; var data: TmuscleRect): boolean; overload; virtual;
    function findPoint(const Name: string; index: integer; var data: TmusclePoint): boolean; overload; virtual;

  public
    property what: cardinal read Getwhat write Setwhat;

    constructor Create; overload; virtual;
    constructor Create(w: cardinal); overload; virtual;
    constructor Create(copyMe: TmuscleMessage); overload; virtual;

    destructor  Destroy;  override;                                                      //V1_4_3_9

    function fieldNames: TStrings;
    procedure Clear;

    procedure copyField(fieldName: string; msg: IPortableMessage); virtual;
    function countFields(fieldType: cardinal = B_ANY_TYPE): integer; overload;

    function countFields(fieldname: string; fieldType: cardinal = B_ANY_TYPE): integer; overload;

    procedure renameField(old, new: string);

    function deleteField(const Name: string; index: integer = 0): boolean;

    class function whatString(w: cardinal): string; //this should really be external.. bloody Java

    //from TmuscleFlattenable
    function isFixedSize: boolean; override;
    function typeCode: cardinal; override;
    function flattenedSize: integer; override;
    function cloneFlat: TmuscleFlattenable; override;
    procedure setEqualTo(setFromMe: TmuscleFlattenable); override; //throws ClassCastException;  ... assign???

    procedure flatten(AOut: TStream); override;
    function allowsTypeCode(code: integer): boolean; override;
    procedure unflatten(ain: TStream; numBytes: integer); override;

    //mainly for testing...
    function toString: string; override;
  end;

  TmuscleMessage = class(TmuscleCustomMessage, ImuscleMessage)
  public
    procedure addString(const Name: string; const data: string); override;
    procedure addInt8(const Name: string; const data: Shortint); override;
    procedure addInt16(const Name: string; const data: SmallInt); override;
    procedure addInt32(const Name: string; const data: LongInt); override;
    procedure addInt64(const Name: string; const data: int64); override;
    procedure addBool(const Name: string; const data: boolean); override;
    procedure addFloat(const Name: string; const data: single); override;
    procedure addDouble(const Name: string; const data: double); override;
    procedure addMessage(const Name: string; data: IPortableMessage); override;
    procedure addRect(const Name: string; data: TmuscleRect); override;
    procedure addPoint(const Name: string; data: TmusclePoint); override;

    function replaceString(const Name: string; const data: string): boolean; override;
    function replaceInt8(const Name: string; const data: ShortInt): boolean; override;
    function replaceInt16(const Name: string; const data: SmallInt): boolean; override;
    function replaceInt32(const Name: string; const data: LongInt): boolean; override;
    function replaceInt64(const Name: string; const data: Int64): boolean; override;
    function replaceBool(const Name: string; const data: boolean): boolean; override;
    function replaceFloat(const Name: string; const data: single): boolean; override;
    function replaceDouble(const Name: string; const data: double): boolean; override;
    function replaceMessage(const Name: string; data: IPortableMessage): boolean; overload; override;
    function replaceRect(const Name: string; data: TmuscleRect): boolean; override;
    function replacePoint(const Name: string; data: TmusclePoint): boolean; override;

    function replaceMessage(const Name: string; index: integer; data: IPortableMessage): boolean; overload; override;

    function findString(const Name: string; var data: string): boolean; overload; override;
    function findInt8(const Name: string; var data: ShortInt): boolean; overload; override;
    function findInt16(const Name: string; var data: SmallInt): boolean; overload; override;
    function findInt32(const Name: string; var data: LongInt): boolean; overload; override;
    function findInt64(const Name: string; var data: Int64): boolean; overload; override;
    function findFloat(const Name: string; var data: single): boolean; overload; override;
    function findDouble(const Name: string; var data: double): boolean; overload; override;
    function findBool(const Name: string; var data: boolean): boolean; overload; override;
    function findMessage(const Name: string; var data: IPortableMessage): boolean; overload; override;
    function findRect(const Name: string; var data: TmuscleRect): boolean; overload; override;
    function findPoint(const Name: string; var data: TmusclePoint): boolean; overload; override;

    function findString(const Name: string; index: integer; var data: string): boolean; overload; override;
    function findInt8(const Name: string; index: integer; var data: ShortInt): boolean; overload; override;
    function findInt16(const Name: string; index: integer; var data: SmallInt): boolean; overload; override;
    function findInt32(const Name: string; index: integer; var data: LongInt): boolean; overload; override;
    function findInt64(const Name: string; index: integer; var data: Int64): boolean; overload; override;
    function findFloat(const Name: string; index: integer; var data: single): boolean; overload; override;
    function findDouble(const Name: string; index: integer; var data: double): boolean; overload; override;
    function findBool(const Name: string; index: integer; var data: boolean): boolean; overload; override;
    function findMessage(const Name: string; index: integer; var data: IPortableMessage): boolean; overload; override;
    function findRect(const Name: string; index: integer; var data: TmuscleRect): boolean; overload; override;
    function findPoint(const Name: string; index: integer; var data: TmusclePoint): boolean; overload; override;

  end;

  //////////////////////////////////////////////////////////////////////////////

  TPortableMessage = class(TmuscleCustomMessage, ImuscleMessage, IPortableMessage)
  protected
    function __Flatten: string; //depricated
    procedure __Unflatten(data: string); //depricated

  public
    //procedure AddVariant(const Name: string; const data: Variant; const datatype: TPortableMessageDataType);
    procedure AddString(const Name: string; const data: string); override;
    procedure AddInteger(const Name: string; const data: integer);
    procedure AddCardinal(const Name: string; const data: cardinal);
    procedure AddBoolean(const Name: string; const data: boolean);
    procedure AddFloat(const Name: string; const data: single); override;
    procedure AddDouble(const Name: string; const data: double); override; //for TDateTime etc		//V1_4_4_x
    procedure AddDataStream(const Name: string; data: TStream);
    procedure AddMessage(const Name: string; data: IPortableMessage); override;

    function ReplaceString(const Name: string; const data: string): boolean; override;
    function ReplaceInteger(const Name: string; const data: integer): boolean;
    function ReplaceCardinal(const Name: string; const data: cardinal): boolean;
    function ReplaceBoolean(const Name: string; const data: boolean): boolean;
    function ReplaceFloat(const Name: string; const data: single): boolean; override;
    function ReplaceDouble(const Name: string; const data: double): boolean; override;			//V1_4_4_x
    function ReplaceMessage(const Name: string; data: IPortableMessage): boolean; override;

    procedure FindVariant(const Name: string; var data: variant);
    function FindString(const Name: string): string; overload;
    function FindInteger(const Name: string): Integer;
    function FindCardinal(const Name: string): Cardinal;
    function FindFloat(const Name: string): single; overload;
    function FindDouble(const Name: string): double; overload;				//V1_4_4_x
    function FindBoolean(const Name: string): boolean;
    function FindDataStream(const Name: string; data: TStream): boolean;
    function FindDataStreamData(const Name: string; toStream: TStream; dataCount: integer): integer;
    function FindMessage(const Name: string): IPortableMessage; overload;

    function GetWhat: Cardinal;
    procedure SetWhat(const Value: Cardinal);
    property What: Cardinal read GetWhat write SetWhat;

    procedure copyFrom(const source: IPortableMessage);
  end;

function GetCharsAsInteger(fchars: string): cardinal; //convenience routine...

//lovely binhex routines
procedure StreamHexToBin(Reader: TStream; Writer: TStream); //convenience routine...
procedure StreamBinToHex(Reader: TStream; Writer: TStream); //convenience routine...

implementation

uses
  MuscleExceptions, windows, Winsock;


function GetCharsAsInteger(fchars: string): cardinal;
var
  ch: array[0..3] of char;
begin
  strlcopy(@ch, pchar(fchars), 4);
  Result := cardinal(ch);
end;

procedure StreamBinToHex(Reader: TStream; Writer: TStream);
const
  BytesPerLine = 32;
var
  I: Integer;
  Count: Longint;
  Buffer: array[0..BytesPerLine - 1] of Char;
  Text: array[0..BytesPerLine * 2 - 1] of Char;
begin
  Count := Reader.Size;
  while Count > 0 do
  begin
    if Count >= 32 then
      I := 32
    else
      I := Count;
    Reader.Read(Buffer, I);
    BinToHex(Buffer, Text, I);
    Writer.Write(Text, I * 2);
    Dec(Count, I);
  end;
end;

procedure StreamHexToBin(Reader: TStream; Writer: TStream);
const
  BytesPerLine = 32;
var
  I, _read: Integer;
  Count: Longint;
  Buffer: array[0..BytesPerLine - 1] of Char;
  Text: array[0..BytesPerLine * 2 - 1] of Char;
begin
  Count := Reader.Size;
  while Count > 0 do
  begin
    if Count >= 32 then
      I := 32
    else
      I := Count;
    Reader.Read(Text, I);
    _read := HexToBin(Text, Buffer, Sizeof(Buffer));
    Writer.Write(Buffer, _read);
    Dec(Count, I);
  end;
end;

//internal data storage

type
  TBDataRec = class(TPersistent)
  public
    name: Shortstring;
    muscledatatype: Cardinal;

    constructor Create(Adatatype: Cardinal); overload;
    destructor  Destroy;  override;                                               //V1_4_3_9

    function GetData: Variant; virtual; abstract;
    procedure SetData(AData: variant); virtual; abstract;

    function DataAsString: string; virtual;

    procedure DataAsStream(s: TStream); virtual;

    property __Data: Variant read GetData write SetData;

    function SizeOf: cardinal; virtual; abstract;
  end;

  TStringBDataRec = class(TBDataRec)
  public
    IsShortString: Boolean;

  end;

  TShortStringBDataRec = class(TStringBDataRec)
  public
    Data: shortstring;
    constructor Create(AData: shortstring); overload;

    function DataAsString: string; override;

    function GetData: Variant; override;
    procedure SetData(AData: variant); override;

    function SizeOf: cardinal; override;
  end;

  TLongStringBDataRec = class(TStringBDataRec)
  public
    Data: string;
    constructor Create(AData: string); overload;

    function DataAsString: string; override;

    function GetData: Variant; override;
    procedure SetData(AData: variant); override;

    function SizeOf: cardinal; override;
  end;

  TFloatBDataRec = class(TBDataRec)
  public
    Data: Single;
    constructor Create(AData: single); overload;

    function GetData: Variant; override;
    procedure SetData(AData: variant); override;

    function DataAsString: string; override;

    function SizeOf: cardinal; override;
  end;

  TDoubleBDataRec = class(TBDataRec)
  public
    Data: Double;
    constructor Create(AData: Double);

    function GetData: Variant; override;
    procedure SetData(AData: variant); override;

    function SizeOf: cardinal; override;
  end;

  TInt8BDataRec = class(TBDataRec)
  public
    Data: shortint;
    constructor Create(AData: shortint);

    function GetData: Variant; override;
    procedure SetData(AData: variant); override;

    function SizeOf: cardinal; override;
  end;

  TInt16BDataRec = class(TBDataRec)
  public
    Data: smallint;
    constructor Create(AData: smallint);

    function GetData: Variant; override;
    procedure SetData(AData: variant); override;

    function SizeOf: cardinal; override;
  end;

  TInt32BDataRec = class(TBDataRec)
  public
    Data: Integer;
    constructor Create(AData: Integer);

    function GetData: Variant; override;
    procedure SetData(AData: variant); override;

    function SizeOf: cardinal; override;
  end;

  TLongBDataRec = class(TBDataRec)
  public
    Data: Int64;
    constructor Create(AData: Int64);
    constructor CreateStr(AData: string);

    function GetData: Variant; override;
    procedure SetData(AData: variant); override;

    function SizeOf: cardinal; override;
  end;


  TBoolBDataRec = class(TBDataRec)
  public
    Data: boolean;

    constructor Create(AData: Boolean); overload;

    function GetData: Variant; override;
    procedure SetData(AData: variant); override;

    function DataAsString: string; override;

    function SizeOf: cardinal; override;
  end;

  TBLOBDataRec = class(TBDataRec)
  public
    Data: TMemoryStream;
    constructor Create(AData: TStream);
    constructor CreateFromHex(AData: string);
    destructor Destroy; override;

    function GetData: Variant; override;
    procedure SetData(AData: variant); override;

    procedure DataAsStream(s: TStream); override;

    function DataAsString: string; override;

    function SizeOf: cardinal; override;
  end;

  TPortableMessageDataRec = class(TBDataRec)
  public
    //Data: TmuscleCustomMessage;
    Data: IPortableMessage;
    constructor Create(AData: IPortableMessage);
    constructor CreateStr(AData: TStream);
    destructor Destroy; override;

    function GetData: Variant; override;
    procedure SetData(AData: variant); override;

    function SizeOf: cardinal; override;
  end;

  TRectDataRec = class(TBDataRec)
  public
    Data: TmuscleRect;
    constructor Create(AData: TmuscleRect);
    constructor CreateStr(AData: TStream);
    destructor Destroy; override;

    function GetData: Variant; override;
    procedure SetData(AData: variant); override;

    function SizeOf: cardinal; override;
  end;

  TPointDataRec = class(TBDataRec)
  public
    Data: TmusclePoint;
    constructor Create(AData: TmusclePoint);
    constructor CreateStr(AData: TStream);
    destructor Destroy; override;

    function GetData: Variant; override;
    procedure SetData(AData: variant); override;

    function SizeOf: cardinal; override;
  end;


// ------------------------------------------------------------------------------------------------
//  TBDataRec
// ------------------------------------------------------------------------------------------------

constructor TBDataRec.Create(Adatatype: Cardinal);
begin
  muscledatatype := Adatatype;
end;


// ------------------------------------------------------------------------------------------------
                                                                                  //new fn V1_4_3_9
destructor TBDataRec.Destroy;
begin
  inherited;
end;

// ------------------------------------------------------------------------------------------------

procedure TBDataRec.DataAsStream(s: TStream);
var
  t: TStringStream;
begin
  t := TStringStream.Create(DataAsString);
  try
    t.position := 0;
    s.CopyFrom(t, t.size);
  finally
    t.free;
  end;
end;

// ------------------------------------------------------------------------------------------------

function TBDataRec.DataAsString: string;
begin
  Result := '';
end;

// ------------------------------------------------------------------------------------------------
//  TShortStringBDataRec
// ------------------------------------------------------------------------------------------------

constructor TShortStringBDataRec.Create(AData: shortstring);
begin
  inherited Create(B_STRING_TYPE);

  IsShortString := true;

  Data := AData;

end;

function TShortStringBDataRec.DataAsString: string;
begin
  result := '';
//  Result := format('%s'#6#6#6'%d'#6#6#6'%s'#7#7#7, [Name, Ord(dataType), Data]);
end;

function TShortStringBDataRec.GetData: Variant;
begin
  Result := Data;
end;

procedure TShortStringBDataRec.SetData(AData: variant);
begin
  Data := VarToStr(AData);
end;

{ TFloatBDataRec }

function TFloatBDataRec.DataAsString: string;
begin
  result := '';
//  Result := format('%s'#6#6#6'%d'#6#6#6'%f'#7#7#7, [Name, Ord(dataType), Data]);
end;

function TFloatBDataRec.GetData: Variant;
begin
  Result := Data;
end;

procedure TFloatBDataRec.SetData(AData: variant);
begin
  Data := AData;
end;

constructor TBoolBDataRec.Create(AData: Boolean);
begin
  inherited Create(B_BOOL_TYPE);

  Data := Adata;
end;

function TBoolBDataRec.DataAsString: string;
begin
  result := '';
//  Result := format('%s'#6#6#6'%d'#6#6#6'%u'#7#7#7, [Name, Ord(dataType), Ord(Data)]);
end;

function TBoolBDataRec.GetData: Variant;
begin
  Result := Data;
end;

procedure TBoolBDataRec.SetData(AData: variant);
begin
  Data := VarAsType(Adata, varBoolean);
end;

constructor TLongStringBDataRec.Create(AData: string);
begin
  inherited Create(B_STRING_TYPE);

  Data := AData;

  IsShortString := false;
end;

function TLongStringBDataRec.DataAsString: string;
begin
  result := '';
//  Result := format('%s'#6#6#6'%d'#6#6#6'%s'#7#7#7, [Name, Ord(dataType), Data]);
end;

function TLongStringBDataRec.GetData: Variant;
begin
  Result := Data;
end;

procedure TLongStringBDataRec.SetData(AData: variant);
begin
  Data := VarToStr(AData);
end;

//------------------------------------------------------------------------------------------------------------------------------------------

//Tmuscle extensions

function TShortStringBDataRec.SizeOf: cardinal;
begin
  Result := length(data);
end;

function TBoolBDataRec.SizeOf: cardinal;
begin
  result := 1; //defined by Tmuscle code...
end;

function TLongStringBDataRec.SizeOf: cardinal;
begin
  Result := length(data);
end;

{ TFloatBDataRec }

constructor TFloatBDataRec.Create(AData: Single);
begin
  inherited Create(B_FLOAT_TYPE);

  Data := AData;
end;

function TFloatBDataRec.SizeOf: cardinal;
begin
  Result := System.sizeof(single);
end;

{ TInt32BDataRec }

constructor TInt32BDataRec.Create(AData: Integer);
begin
  inherited Create(B_INT32_TYPE);

  Data := AData;
end;

function TInt32BDataRec.GetData: Variant;
begin
  Result := Data;
end;

procedure TInt32BDataRec.SetData(AData: variant);
begin
  Data := AData;
end;

function TInt32BDataRec.SizeOf: cardinal;
begin
  result := System.sizeof(integer);
end;

{ TInt8BDataRec }

constructor TInt8BDataRec.Create(AData: shortint);
begin
  inherited Create(B_INT8_TYPE);

  Data := AData;
end;

function TInt8BDataRec.GetData: Variant;
begin
  Result := Data;
end;

procedure TInt8BDataRec.SetData(AData: variant);
begin
  Data := AData;
end;

function TInt8BDataRec.SizeOf: cardinal;
begin
  result := System.sizeof(shortint);
end;

{ TInt16BDataRec }

constructor TInt16BDataRec.Create(AData: smallint);
begin
  inherited Create(B_INT16_TYPE);

  Data := AData;
end;

function TInt16BDataRec.GetData: Variant;
begin
  Result := Data;
end;

procedure TInt16BDataRec.SetData(AData: variant);
begin
  Data := AData;
end;

function TInt16BDataRec.SizeOf: cardinal;
begin
  Result := System.sizeof(smallint);
end;

{ TMessageDataRec }

constructor TPortableMessageDataRec.Create(AData: IPortableMessage);
begin
  inherited Create(B_MESSAGE_TYPE);

  //Data := AData;
  Data := TPortableMessage.Create;

  (Data).copyFrom(AData);
end;

constructor TPortableMessageDataRec.CreateStr(AData: TStream);
begin
  inherited Create(B_MESSAGE_TYPE);

  Data := TPortableMessage.Create;
  AData.Position := 0;
  Data.Unflatten(AData, AData.Size);
end;

destructor TPortableMessageDataRec.Destroy;
begin
  //Data.free;                                                                                   //we have to own this as we constructed it...

  Data := nil;

  inherited;
end;

function TPortableMessageDataRec.GetData: Variant;
begin
  //Result := Data.Flatten;
end;

procedure TPortableMessageDataRec.SetData(AData: variant);
var
  s: string;
  ss: TStringStream;
begin
  s := VarToStr(AData);

  if not Assigned(Data) then
    Data := TPortableMessage.Create;

  ss := TStringStream.create(s);
  try
    ss.position := 0;
    Data.Unflatten(ss, ss.size);
  finally
    ss.free;
  end;
end;

function TPortableMessageDataRec.SizeOf: cardinal;
begin
  result := Data.flattenedSize;
end;

// ------------------------------------------------------------------------------------------------
//  TmuscleCustomMessage
// ------------------------------------------------------------------------------------------------

constructor TmuscleCustomMessage.Create;
begin
  ensureFieldTableAllocated;
end;

// ------------------------------------------------------------------------------------------------

constructor TmuscleCustomMessage.Create(w: cardinal);
begin
  Create;
  Fwhat := w;
end;

// ------------------------------------------------------------------------------------------------

constructor TmuscleCustomMessage.Create(copyMe: TmuscleMessage);
begin
  Create;
  setEqualTo(copyMe);
end;

// ------------------------------------------------------------------------------------------------

procedure TmuscleCustomMessage.ensureFieldTableAllocated;
begin
  if (_fieldTable = nil) then
    _fieldTable := TStringList.Create;
end;

// ------------------------------------------------------------------------------------------------
                                                                                  //new fn V1_4_3_9
destructor TmuscleCustomMessage.Destroy;
begin
  Clear;

  if Assigned (_fieldTable) then      //oops.....
    FreeAndNil (_fieldTable);

  inherited;
end;

// ------------------------------------------------------------------------------------------------

procedure TmuscleCustomMessage.Clear;
var
  i: integer;

begin
  if not Assigned (_fieldTable) then                                                     //V1_4_3_9
    exit;                                                                                //V1_4_3_9

  for i := _fieldTable.Count - 1 downto 0 do
    TObject(_fieldTable.Objects[i]).Free;

  _fieldTable.Clear;
end;

// ------------------------------------------------------------------------------------------------

procedure TmuscleCustomMessage.copyField(fieldName: string; msg: IPortableMessage);
//var
//  count, i, ftype: integer;
begin

//we need to know they type code of any given field for this to work...

//
//  count := (msg as ImuscleMessage).countFields(fieldName);
//
//  if count > 0 then
//    ftype :=  (msg as ImuscleMessage).
//  for i := 0 to count -1 do
//  begin
//
//  end;
end;

function TmuscleCustomMessage.allowsTypeCode(code: integer): boolean;
begin
  Result := (code = B_MESSAGE_TYPE);
end;

function TmuscleCustomMessage.cloneFlat: TmuscleFlattenable;
var
  clone: TmuscleMessage;
begin
  clone := TmuscleMessage.Create;
  clone.setEqualTo(Self);
  Result := clone;
end;

function TmuscleCustomMessage.fieldNames: TStrings;
begin
  //I'm sure I'm going to hate and change this...

  result := _fieldTable;
end;

procedure TmuscleCustomMessage.flatten(AOut: TStream);
var
  protocol, fieldcount, i, l, t: integer;
  field: TmuscleMessageField;

  procedure WriteString(s: string; str: TStream);
  var
    i: integer;
    nullch: char;
  begin
    for i := 1 to length(s) do
      str.Write(s[i], sizeof(char));

    nullch := #0;
    str.Write(nullch, sizeof(char));
  end;

begin
  // Format:  0. Protocol revision number (4 bytes, always set to CURRENT_PROTOCOL_VERSION)
  //          1. 'what' code (4 bytes)
  //          2. Number of entries (4 bytes)
  //          3. Entry name length (4 bytes)
  //          4. Entry name string (flattened String)
  //          5. Entry type code (4 bytes)
  //          6. Entry data length (4 bytes)
  //          7. Entry data (n bytes)
  //          8. loop to 3 as necessary
  try
    protocol := CURRENT_PROTOCOL_VERSION; //0
    AOut.Write(protocol, SizeOf(Integer)); //1
    AOut.Write(Fwhat, SizeOf(integer)); //2

    fieldcount := countFields;
    AOut.Write(fieldcount, SizeOf(Integer)); //3

    for i := 0 to _fieldTable.Count - 1 do
    begin
      l := length(_fieldTable[i]) + 1;

      AOut.Write(l, SizeOf(Integer)); //4
      WriteString(_fieldTable[i], AOut); //5

      field := TmuscleMessageField(_fieldTable.Objects[i]);

      t := field.typecode;
      AOut.Write(t, sizeof(integer)); //6

      t := field.flattenedSize;
      AOut.Write(t, sizeof(integer)); //7

      field.flatten(Aout); //8  ...hmmm, recursive..
    end;

  except
    inherited;
  end;
end;

function TmuscleCustomMessage.flattenedSize: integer;
var
  field: TmuscleMessageField;
  i: integer;
begin
  Result := 4 + 4 + 4; // Revision + Number-Of-Entries + what

  if (_fieldTable <> nil) then
  begin
    for i := 0 to _fieldTable.Count - 1 do
    begin
      field := TmuscleMessageField(_fieldTable.Objects[i]);
      Result := Result + // namelength + namedata + typecode + entrydata length + entrydata
      4 + (length(_fieldTable[i]) + 1) +
        4 + 4;
      result := result + field.flattenedSize;
    end;
  end;
end;

function TmuscleCustomMessage.isFixedSize: boolean;
begin
  Result := false;
end;

procedure TmuscleCustomMessage.setEqualTo(setFromMe: TmuscleFlattenable);
//var
//  copyMe: TmuscleMessage;
//  fields: TStrings;
//  i: integer;
begin
  inherited;

//  clear;
//
//  copyMe := TmuscleMessage(setFromMe);
//
//  Fwhat := copyMe.what;
//  fields := copyMe.fieldNames;
//  try
//    for i := 0 to fields.count - 1 do
//    try
//      copyMe.copyField(fields[i], TmuscleMessage(self));
//    except
//      on E: Exception do
//        raise Exception.Create('Field not found'#13#10'Original Error: ' + E.Message);
//    end;
//  finally
//    Fields.Free;
//  end;

end;

procedure TmuscleCustomMessage.Setwhat(const Value: cardinal);
begin
  Fwhat := Value;
end;

function TmuscleCustomMessage.toString: string;
var
  i: integer;
begin
  Result := 'Message:  what=''' + whatString(Fwhat) + ''' (' + IntToStr(Fwhat) + '), countFields=' +
    IntToStr(countFields) + ', flattenedSize=' + intToStr(flattenedSize) + #13#10;

  if (_fieldTable = nil) then
    exit; //avoid an exception, I guess....

  for i := 0 to _fieldTable.Count - 1 do
  begin
    Result := Result + '   ' + _fieldTable[i] + ': ' + TmuscleMessageField(_fieldTable.Objects[i]).toString;
  end;
end;

function TmuscleCustomMessage.typeCode: cardinal;
begin
  Result := B_MESSAGE_TYPE;
end;

procedure TmuscleCustomMessage.unflatten(ain: TStream; numBytes: integer);
var
  protocolVersion, numEntries, fieldNameLength, i, t: integer;
  s: pchar;
  field: TmuscleMessageField;
begin
  (* clear();
     int protocolVersion = in.readInt();
     if ((protocolVersion > CURRENT_PROTOCOL_VERSION)||(protocolVersion < OLDEST_SUPPORTED_PROTOCOL_VERSION)) throw new UnflattenFormatException("Version mismatch error");
     what = in.readInt();
     int numEntries = in.readInt();
     byte [] stringBuf = null;
     if (numEntries > 0) ensureFieldTableAllocated();
     for (int i=0; i<numEntries; i++)
     {
        int fieldNameLength = in.readInt();
        if ((stringBuf == null)||(stringBuf.length < fieldNameLength)) stringBuf = new byte[((fieldNameLength>5)?fieldNameLength:5)*2];
        in.readFully(stringBuf, 0, fieldNameLength);
        MessageField field = getCreateOrReplaceField(new String(stringBuf, 0, fieldNameLength-1), in.readInt());
        field.unflatten(in, in.readInt());
        _fieldTable.put(new String(stringBuf, 0, fieldNameLength-1), field);
     } *)

  // Format:  0. Protocol revision number (4 bytes, always set to CURRENT_PROTOCOL_VERSION)
  //          1. 'what' code (4 bytes)
  //          2. Number of entries (4 bytes)
  //          3. Entry name length (4 bytes)
  //          4. Entry name string (flattened String)
  //          5. Entry type code (4 bytes)
  //          6. Entry data length (4 bytes)
  //          7. Entry data (n bytes)
  //          8. loop to 3 as necessary

  try
    clear;

    ain.Read(protocolVersion, sizeof(Integer)); //0
    if ((protocolVersion > CURRENT_PROTOCOL_VERSION) or (protocolVersion < OLDEST_SUPPORTED_PROTOCOL_VERSION)) then
      raise Exception.Create('Version mismatch error');

    ain.Read(Fwhat, sizeof(Integer)); //1

    ain.Read(numEntries, sizeof(integer)); //2
    if (numEntries > 0) then
      ensureFieldTableAllocated;

    s := nil;
    try

      for i := 0 to numEntries - 1 do
      begin
        ain.Read(fieldNameLength, sizeof(Integer)); //3
        if (s = nil) or (strlen(s) < Cardinal(fieldNameLength)) then
        begin //warning...
          //this sucks...
          if not (s = nil) then
            strdispose(s);

          if (fieldNameLength <= 5) then
            s := stralloc(10)
          else
            s := stralloc(fieldNameLength * 2);
        end;

        ain.Read(s^, fieldNameLength); //4
        ain.Read(t, sizeof(Integer)); //5
        field := getCreateOrReplaceField(s, t);

        ain.Read(t, sizeof(Integer)); //6
        field.unflatten(ain, t); //7
        _fieldTable.AddObject(s, field);
      end;

    finally
      if (s <> nil) then
        strdispose(s);
    end;
  except
    inherited;
  end;

end;

class function TmuscleCustomMessage.whatString(w: cardinal): string;
var
  ch: array[0..4] of char;
begin
  SetLength(Result, 4);  //not syre this is still needed...?

  w := ntohl(w);

  move(w, ch, sizeof(cardinal));

  ch[4] := #0;
  result := ch;

  if (ch[0] = #0) and (ch[1] = #0) and (ch[2] = #0) and (ch[3] = #0) then
    Result := 'XXXX';
end;

function TmuscleCustomMessage.countFields(fieldType: cardinal): integer;
var
  i: integer;
  field: TmuscleMessageField;
begin
  if (_fieldTable = nil) then
  begin
    Result := 0;
  end
  else if (fieldType = B_ANY_TYPE) then
    Result := _fieldTable.Count
  else
  begin
    Result := 0;

    for i := 0 to _fieldTable.Count do
    begin
      field := TmuscleMessageField(_fieldTable.Objects[i]);
      if (field.typeCode = fieldType) then
        inc(Result);
    end;
  end;
end;

procedure TmuscleCustomMessage.renameField(old, new: string);
var
  i: integer;
begin
  i := _fieldTable.IndexOf(old);
  if (i > -1) then
  begin
    _fieldTable[i] := new;
  end
  else
    raise Exception.Create('Field not found');

end;

// ------------------------------------------------------------------------------------------------

function TmuscleCustomMessage.getCreateOrReplaceField(fieldname: string; fieldType: cardinal): TmuscleMessageField;
var
  field: TmuscleMessageField;
  idx: integer;
begin
  //Result := nil;
  ensureFieldTableAllocated;

  idx := _fieldTable.IndexOf(fieldname);

  if (idx > -1) then
  begin
    field := TmuscleMessageField(_fieldTable.Objects[idx]);

    if not (field.typeCode = fieldType) then
    begin
      //delete the item
      _fieldTable.Delete(idx);
      field.Free;

      Result := getCreateOrReplaceField(fieldName, fieldType);
    end
    else
      Result := field;
  end
  else
  begin
    Result := TmuscleMessageField.Create(fieldtype);
  end;
end;

// ------------------------------------------------------------------------------------------------

procedure TmuscleCustomMessage.addBool(const Name: string; const data: boolean);
var
  idx: integer;
  fld: TmuscleMessageField;
  b: TBDataRec;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    b := TBoolBDataRec.Create(Data);
    fld.PayLoad.Add(b);
  end
  else
  begin
    b := TBoolBDataRec.Create(Data);
    fld := TmuscleMessageField.Create(b.muscledatatype);
    fld.PayLoad.Add(b);
    _fieldTable.AddObject(name, fld);
  end;
end;

procedure TmuscleCustomMessage.addFloat(const Name: string; const data: single);
var
  idx: integer;
  fld: TmuscleMessageField;
  b: TBDataRec;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    b := TFloatBDataRec.Create(Data);
    fld.PayLoad.Add(b);
  end
  else
  begin
    b := TFloatBDataRec.Create(Data);
    fld := TmuscleMessageField.Create(b.muscledatatype);
    fld.PayLoad.Add(b);
    _fieldTable.AddObject(name, fld);
  end;

end;

procedure TmuscleCustomMessage.addInt16(const Name: string; const data: SmallInt);
var
  idx: integer;
  fld: TmuscleMessageField;
  b: TBDataRec;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    b := TInt16BDataRec.Create(Data);
    fld.PayLoad.Add(b);
  end
  else
  begin
    b := TInt16BDataRec.Create(Data);
    fld := TmuscleMessageField.Create(b.muscledatatype);
    fld.PayLoad.Add(b);
    _fieldTable.AddObject(name, fld);
  end;
end;

procedure TmuscleCustomMessage.addInt32(const Name: string; const data: Integer);
var
  idx: integer;
  fld: TmuscleMessageField;
  b: TBDataRec;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    b := TInt32BDataRec.Create(Data);
    fld.PayLoad.Add(b);
  end
  else
  begin
    b := TInt32BDataRec.Create(Data);
    fld := TmuscleMessageField.Create(b.muscledatatype);
    fld.PayLoad.Add(b);
    _fieldTable.AddObject(name, fld);
  end;

end;

procedure TmuscleCustomMessage.addInt64(const Name: string; const data: int64);
var
  idx: integer;
  fld: TmuscleMessageField;
  b: TBDataRec;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    b := TLongBDataRec.Create(Data);
    fld.PayLoad.Add(b);
  end
  else
  begin
    b := TLongBDataRec.Create(Data);
    fld := TmuscleMessageField.Create(b.muscledatatype);
    fld.PayLoad.Add(b);
    _fieldTable.AddObject(name, fld);
  end;

end;

procedure TmuscleCustomMessage.addInt8(const Name: string; const data: Shortint);
var
  idx: integer;
  fld: TmuscleMessageField;
  b: TBDataRec;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    b := TInt8BDataRec.Create(Data);
    fld.PayLoad.Add(b);
  end
  else
  begin
    b := TInt8BDataRec.Create(Data);
    fld := TmuscleMessageField.Create(b.muscledatatype);
    fld.PayLoad.Add(b);
    _fieldTable.AddObject(name, fld);
  end;
end;

procedure TmuscleCustomMessage.addMessage(const Name: string; data: IPortableMessage);
var
  idx: integer;
  fld: TmuscleMessageField;
  b: TBDataRec;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    b := TPortableMessageDataRec.Create(Data);
    fld.PayLoad.Add(b);
  end
  else
  begin
    b := TPortableMessageDataRec.Create(Data);
    fld := TmuscleMessageField.Create(b.muscledatatype);
    fld.PayLoad.Add(b);
    _fieldTable.AddObject(name, fld);
  end;
end;

procedure TmuscleCustomMessage.addString(const Name, data: string);
var
  idx: integer;
  fld: TmuscleMessageField;
  b: TBDataRec;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    b := TLongStringBDataRec.Create(Data);
    fld.PayLoad.Add(b);
  end
  else
  begin
    b := TLongStringBDataRec.Create(Data);
    fld := TmuscleMessageField.Create(b.muscledatatype);
    fld.PayLoad.Add(b);
    _fieldTable.AddObject(name, fld);
  end;
end;

function TmuscleCustomMessage.replaceBool(const Name: string; const data: boolean): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      TBoolBDataRec(fld.Payload[0]).Data := Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.replaceFloat(const Name: string; const data: single): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      TFloatBDataRec(fld.Payload[0]).Data := Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.replaceInt16(const Name: string; const data: SmallInt): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      TInt16BDataRec(fld.Payload[0]).Data := Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.replaceInt32(const Name: string; const data: Integer): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      TInt32BDataRec(fld.Payload[0]).Data := Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.replaceInt64(const Name: string; const data: Int64): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      TLongBDataRec(fld.Payload[0]).Data := Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.replaceInt8(const Name: string; const data: ShortInt): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      TInt8BDataRec(fld.Payload[0]).Data := Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.replaceMessage(const Name: string; data: IPortableMessage): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
  m: IPortableMessage;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      try
        m := TPortableMessageDataRec(fld.Payload[0]).Data;
      finally
        m := nil;
      end;

      TPortableMessageDataRec(fld.Payload[0]).Data := Data;

      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.replaceString(const Name, data: string): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      TLongStringBDataRec(fld.Payload[0]).Data := Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

procedure TmuscleCustomMessage.addDouble(const Name: string; const data: double);
var
  idx: integer;
  fld: TmuscleMessageField;
  b: TBDataRec;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    b := TDoubleBDataRec.Create(Data);
    fld.PayLoad.Add(b);
  end
  else
  begin
    b := TDoubleBDataRec.Create(Data);
    fld := TmuscleMessageField.Create(b.muscledatatype);
    fld.PayLoad.Add(b);
    _fieldTable.AddObject(name, fld);
  end;

end;

function TmuscleCustomMessage.replaceDouble(const Name: string; const data: double): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      TDoubleBDataRec(fld.Payload[0]).Data := Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

procedure TmuscleCustomMessage.addPoint(const Name: string; data: TmusclePoint);
var
  idx: integer;
  fld: TmuscleMessageField;
  b: TBDataRec;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    b := TPointDataRec.Create(Data);
    fld.PayLoad.Add(b);
  end
  else
  begin
    b := TPointDataRec.Create(Data);
    fld := TmuscleMessageField.Create(b.muscledatatype);
    fld.PayLoad.Add(b);
    _fieldTable.AddObject(name, fld);
  end;

end;

procedure TmuscleCustomMessage.addRect(const Name: string; data: TmuscleRect);
var
  idx: integer;
  fld: TmuscleMessageField;
  b: TBDataRec;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    b := TRectDataRec.Create(Data);
    fld.PayLoad.Add(b);
  end
  else
  begin
    b := TRectDataRec.Create(Data);
    fld := TmuscleMessageField.Create(b.muscledatatype);
    fld.PayLoad.Add(b);
    _fieldTable.AddObject(name, fld);
  end;

end;

function TmuscleCustomMessage.countFields(fieldname: string; fieldType: cardinal): integer;
var
  i: integer;
  field: TmuscleMessageField;
begin
  if (_fieldTable = nil) or (_fieldTable.Count = 0) then
  begin
    Result := 0;
  end
  else
  begin
    Result := 0;

    for i := 0 to _fieldTable.Count - 1 do
    begin
      field := TmuscleMessageField(_fieldTable.Objects[i]);
      if (AnsiCompareText(_fieldTable[i], fieldname) = 0) and ((field.typeCode = fieldType) or (fieldtype = B_ANY_TYPE)) then			//V1_4_4_x
        inc(Result, TmuscleMessageField(_fieldTable.Objects[i]).Count);
    end;
  end;

end;

function TmuscleCustomMessage.findBool(const Name: string; index: integer; var data: boolean): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) and (index > -1) and (index < fld.Payload.Count) then
    begin
      Data := TBoolBDataRec(fld.Payload[index]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findBool(const Name: string; var data: boolean): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      Data := TBoolBDataRec(fld.Payload[0]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findDouble(const Name: string; index: integer; var data: double): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) and (index > -1) and (index < fld.Payload.Count) then
    begin
      Data := TDoubleBDataRec(fld.Payload[index]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findFloat(const Name: string; var data: single): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      Data := TFloatBDataRec(fld.Payload[0]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findFloat(const Name: string; index: integer; var data: single): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) and (index > -1) and (index < fld.Payload.Count) then
    begin
      Data := TFloatBDataRec(fld.Payload[index]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findInt16(const Name: string; var data: SmallInt): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      Data := TInt16BDataRec(fld.Payload[0]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findInt16(const Name: string; index: integer; var data: SmallInt): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) and (index > -1) and (index < fld.Payload.Count) then
    begin
      Data := TInt16BDataRec(fld.Payload[index]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findInt32(const Name: string; index: integer; var data: Integer): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) and (index > -1) and (index < fld.Payload.Count) then
    begin
      Data := TInt32BDataRec(fld.Payload[index]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findInt32(const Name: string; var data: Integer): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      Data := TInt32BDataRec(fld.Payload[0]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findInt64(const Name: string; var data: Int64): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      Data := TLongBDataRec(fld.Payload[0]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findInt64(const Name: string; index: integer; var data: Int64): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) and (index > -1) and (index < fld.Payload.Count) then
    begin
      Data := TLongBDataRec(fld.Payload[index]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findInt8(const Name: string; index: integer; var data: ShortInt): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) and (index > -1) and (index < fld.Payload.Count) then
    begin
      Data := TInt8BDataRec(fld.Payload[index]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findInt8(const Name: string; var data: ShortInt): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      Data := TInt8BDataRec(fld.Payload[0]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findMessage(const Name: string; index: integer; var data: IPortableMessage): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) and (index > -1) and (index < fld.Payload.Count) then
    begin
      Data := TPortableMessageDataRec(fld.Payload[index]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findMessage(const Name: string; var data: IPortableMessage): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      Data := TPortableMessageDataRec(fld.Payload[0]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findPoint(const Name: string; index: integer; var data: TmusclePoint): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) and (index > -1) and (index < fld.Payload.Count) then
    begin
      Data := TPointDataRec(fld.Payload[index]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findPoint(const Name: string; var data: TmusclePoint): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      Data := TPointDataRec(fld.Payload[0]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;

end;

function TmuscleCustomMessage.findRect(const Name: string; index: integer; var data: TmuscleRect): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) and (index > -1) and (index < fld.Payload.Count) then
    begin
      Data := TRectDataRec(fld.Payload[index]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findRect(const Name: string; var data: TmuscleRect): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      Data := TRectDataRec(fld.Payload[0]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findString(const Name: string; var data: string): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      Data := copy(TLongStringBDataRec(fld.Payload[0]).Data, 1, length(TLongStringBDataRec(fld.Payload[0]).Data)); //I know, I know...
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findString(const Name: string; index: integer; var data: string): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) and (index > -1) and (index < fld.Payload.Count) then
    begin
      Data := copy(TLongStringBDataRec(fld.Payload[index]).Data, 1, length(TLongStringBDataRec(fld.Payload[index]).Data)); //I know, I know...
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.replacePoint(const Name: string; data: TmusclePoint): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
  m: TmusclePoint;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      m := TPointDataRec(fld.Payload[0]).Data;
      m.free;

      TPointDataRec(fld.Payload[0]).Data := Data;

      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.replaceRect(const Name: string; data: TmuscleRect): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
  m: TmuscleRect;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      m := TRectDataRec(fld.Payload[0]).Data;
      m.free;

      TRectDataRec(fld.Payload[0]).Data := Data;

      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.findDouble(const Name: string; var data: double): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      Data := TDoubleBDataRec(fld.Payload[0]).Data;
      Result := True;
      exit;
    end;
  end;

  Result := False;

end;

function TmuscleCustomMessage.replaceMessage(const Name: string; index: integer; data: IPortableMessage): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
  m: IPortableMessage;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) and (index > -1) and (index < fld.Payload.Count) then
    begin
      m := TPortableMessageDataRec(fld.Payload[index]).Data;

      TPortableMessageDataRec(fld.Payload[index]).Data := Data;

      if (m <> data) then
        m := nil;

      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TmuscleCustomMessage.deleteField(const Name: string; index: integer): boolean;
var
  fld: TmuscleMessageField;
  idx: integer;
begin
  idx := _fieldTable.IndexOf(name);

  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);

    if (fld.PayLoad.Count <= 0) or (index < 0) or (index >= fld.PayLoad.Count) then
    begin
      Result := false;
      Exit;
    end
    else
    begin
      //remove field
      fld.PayLoad.Delete(index); //object list cleans up for us...

      //is the fld now empty?
      if (fld.PayLoad.Count = 0) then
      begin
        //yes... we should delete it...
        _fieldTable.delete(idx);
        fld.free;
      end;

      Result := true;
    end;
  end
  else
    Result := false;
end;

function TmuscleCustomMessage.Getwhat: cardinal;
begin
  Result := Fwhat;
end;

// ------------------------------------------------------------------------------------------------
//  TmuscleMessageField
// ------------------------------------------------------------------------------------------------

constructor TmuscleMessageField.Create;
begin
  FPayload := TObjectList.Create(True);

  //  if DebugMemLogOn then
  //    OutputDebugString ('++++ TmuscleMessageField.Create');
end;

// ------------------------------------------------------------------------------------------------

constructor TmuscleMessageField.Create(AtypeCode: cardinal);
begin
  Create;
  FtypeCode := AtypeCode;
end;

// ------------------------------------------------------------------------------------------------

destructor TmuscleMessageField.Destroy;
begin
  //  if DebugMemLogOn then
  //    OutputDebugString ('---- TmuscleMessageField.Free');

  FPayload.Free;
  inherited;
end;

// ------------------------------------------------------------------------------------------------

function TmuscleMessageField.allowsTypeCode(code: integer): boolean;
begin
  result := (cardinal(code) = FtypeCode);
end;

function TmuscleMessageField.cloneFlat: TmuscleFlattenable;
begin
  raise Exception.Create('cloneFlat is not supported for TmuscleMessageField');
end;

function TmuscleMessageField.Count: integer;
begin
  result := FPayload.Count;
end;

procedure TmuscleMessageField.flatten(AOut: TStream);
var
  i: integer;
  b: byte;
  i8: shortint;
  i16: smallint;
  i32: longint;
  i64: int64;
  f: single;
  d: double;
  m: IPortableMessage;
  s: pchar;
begin
  try
    if FTypeCode = B_STRING_TYPE then						//V1_4_4_x START
    begin
      //DOH!! B_STRING_TYPE has a hidden item count...big bug..
      i32 := FPayLoad.Count;
      AOut.WriteBuffer(i32, sizeof(longint)); //write count
    end;							//V1_4_4_x FINISH

    for i := 0 to FPayLoad.Count - 1 do
      case FtypeCode of
        B_BOOL_TYPE:
          begin
            b := ord(TBoolBDataRec(FPayLoad[i]).Data);
            AOut.WriteBuffer(b, sizeof(byte));
          end;

        B_INT8_TYPE:
          begin
            i8 := TInt8BDataRec(FPayLoad[i]).Data;
            AOut.WriteBuffer(i8, sizeof(shortint));
          end;

        B_INT16_TYPE:
          begin
            i16 := TInt16BDataRec(FPayLoad[i]).Data;
            AOut.WriteBuffer(i16, sizeof(smallint));
          end;

        B_INT32_TYPE:
          begin
            i32 := TInt32BDataRec(FPayLoad[i]).Data;
            AOut.WriteBuffer(i32, sizeof(longint));
          end;

        B_INT64_TYPE:
          begin
            i64 := TLongBDataRec(FPayLoad[i]).Data;
            AOut.WriteBuffer(i64, sizeof(int64));
          end;

        B_FLOAT_TYPE:
          begin
            f := TFloatBDataRec(FPayLoad[i]).Data;
            AOut.WriteBuffer(f, sizeof(single));
          end;

        B_DOUBLE_TYPE:
          begin
            d := TDoubleBDataRec(FPayLoad[i]).Data;
            AOut.WriteBuffer(d, sizeof(double));
          end;

        B_POINT_TYPE:
          begin
            TmusclePoint(TPointDataRec(FPayLoad[i]).data).flatten(AOut);
          end;

        B_RECT_TYPE:
          begin
            TmuscleRect(TPointDataRec(FPayLoad[i]).data).flatten(AOut);
          end;

        B_MESSAGE_TYPE:
          begin
            m := (TPortableMessageDataRec(FPayLoad[i]).Data);

            i32 := m.flattenedSize;
            AOut.WriteBuffer(i32, sizeof(longint)); //write size

            m.flatten(AOut); //write actual message...
          end;

        B_STRING_TYPE:
          begin
            i32 := length(TLongStringBDataRec(FPayLoad[i]).Data) + 1;
            s := stralloc(i32);
            try
              StrPCopy(s, TLongStringBDataRec(FPayLoad[i]).Data);
              s[i32 - 1] := #0;
              AOut.WriteBuffer(i32, sizeof(longint)); //write size
              AOut.WriteBuffer(s^, i32); //write size
            finally
              StrDispose(s);
            end;
          end;

      else
        (* TODO
           byte [][] array = (byte[][]) _payload;
           out.writeInt(_numItems);
           for (int i=0; i<_numItems; i++)
           {
              out.writeInt(array[i].length);
              out.write(array[i]);
           }
        *)
        raise Exception.Create('Unimplemented');

      end;

  except
    inherited;
  end;
end;

function TmuscleMessageField.flattenedItemSize: integer;
begin
  case FTypeCode of
    B_BOOL_TYPE, B_INT8_TYPE:
      Result := 1;

    B_INT16_TYPE:
      Result := 2;

    B_FLOAT_TYPE, B_INT32_TYPE:
      Result := 4;

    B_INT64_TYPE, B_DOUBLE_TYPE, B_POINT_TYPE:
      Result := 8;

    B_RECT_TYPE:
      Result := 16;
  else
    Result := 0;
  end;
end;

function TmuscleMessageField.flattenedSize: integer;
var
  i: integer;
begin
  Result := 0;

  case FtypeCode of
    B_BOOL_TYPE, B_INT8_TYPE, B_INT16_TYPE, B_FLOAT_TYPE, B_INT32_TYPE,
      B_INT64_TYPE, B_DOUBLE_TYPE, B_POINT_TYPE, B_RECT_TYPE:
      Result := FPayload.Count * flattenedItemSize;

    B_MESSAGE_TYPE:
      begin
        // there is no number-of-items field for B_MESSAGE_TYPE (for historical reasons, sigh)
        //Message array[] = (Message[]) _payload;
        for i := 0 to FPayload.Count - 1 do
        begin
          Result := Result + 4 + TPortableMessageDataRec(FPayload[i]).Data.flattenedSize; // 4 for the size int
        end;
      end;

    B_STRING_TYPE:
      begin
        Result := 4; // for the number-of-items field
        //String array[] = (String[]) _payload;
        //try {
        //   for (int i=0; i<_numItems; i++) ret += 4 + array[i].getBytes("UTF8").length + 1;   // 4 for the size int, 1 for the nul byte
        //} catch (UnsupportedEncodingException uee) {
        //   for (int i=0; i<_numItems; i++) ret += 4 + array[i].length() + 1;   // 4 for the size int, 1 for the nul byte
        //}

        //UTF8 - todo...
        for i := 0 to FPayload.Count - 1 do
        begin
{$WARNINGS OFF}
          Result := Result + 4 + TLongStringBDataRec(FPayload[i]).Sizeof + 1;
{$WARNINGS ON}
        end;
      end;

  else
    Result := 4; // for the number-of-items field
    //byte [][] array = (byte[][]) _payload;
    //for (int i=0; i<_numItems; i++) ret += 4 + array[i].length;   // 4 for the size int
    for i := 0 to FPayload.Count - 1 do
    begin
{$WARNINGS OFF}
      Result := Result + 4 + TBDataRec(FPayload[i]).SizeOf; //hmmm... will this work???
{$WARNINGS ON}
    end;
  end;
end;

function TmuscleMessageField.isFixedSize: boolean;
begin
  Result := false;
end;

procedure TmuscleMessageField.setEqualTo(setFromMe: TmuscleFlattenable);
begin
  raise Exception.Create('Unsupported');
end;

procedure TmuscleMessageField.SetPayLoad(const Value: TList);
begin
  FPayLoad := Value;
end;

function TmuscleMessageField.toString: string;
var
  i: integer;

const
  SEPERATOR = #9#9'(idx: %d, typecode: %s) : ';

  function getSeperator(idx: integer): string;
  begin
    Result := format(SEPERATOR, [idx, TmuscleMessage.whatString(FtypeCode)]);
  end;
begin
  { TODO : this would be bloody useful... }

  Result := '';
  try
    for i := 0 to FPayLoad.Count - 1 do
      case FtypeCode of
        B_BOOL_TYPE:
          begin
            //b := ord(TBoolBDataRec(FPayLoad[i]).Data);
            case TBoolBDataRec(FPayLoad[i]).Data of
              true:
                Result := Result + getSeperator(i) + 'True' + #13#10;
              false:
                Result := Result + getSeperator(i) + 'False' + #13#10;
            end;
          end;

        B_INT8_TYPE:
          begin
            //i8 := TInt8BDataRec(FPayLoad[i]).Data;
            //AOut.WriteBuffer(i8, sizeof(shortint));
            Result := Result + getSeperator(i) + format('%d', [TInt8BDataRec(FPayLoad[i]).Data]) + #13#10;
          end;

        B_INT16_TYPE:
          begin
            //i16 := TInt16BDataRec(FPayLoad[i]).Data;
            //AOut.WriteBuffer(i16, sizeof(smallint));
            Result := Result + getSeperator(i) + format('%d', [TInt16BDataRec(FPayLoad[i]).Data]) + #13#10;
          end;

        B_INT32_TYPE:
          begin
            //i32 := TInt32BDataRec(FPayLoad[i]).Data;
            //AOut.WriteBuffer(i32, sizeof(longint));
            Result := Result + getSeperator(i) + format('%d', [TInt32BDataRec(FPayLoad[i]).Data]) + #13#10;
          end;

        B_INT64_TYPE:
          begin
            //i64 := TLongBDataRec(FPayLoad[i]).Data;
            //AOut.WriteBuffer(i64, sizeof(int64));
            Result := Result + getSeperator(i) + format('%d', [TLongBDataRec(FPayLoad[i]).Data]) + #13#10;
          end;

        B_FLOAT_TYPE:
          begin
            //f := TFloatBDataRec(FPayLoad[i]).Data;
            //AOut.WriteBuffer(f, sizeof(single));
            Result := Result + getSeperator(i) + format('%f', [TFloatBDataRec(FPayLoad[i]).Data]) + #13#10;
          end;

        B_DOUBLE_TYPE:
          begin
            //d := TDoubleBDataRec(FPayLoad[i]).Data;
            //AOut.WriteBuffer(d, sizeof(double));
            Result := Result + getSeperator(i) + format('%f', [TDoubleBDataRec(FPayLoad[i]).Data]) + #13#10;
          end;

        B_POINT_TYPE:
          begin
            //TmusclePoint(TPointDataRec(FPayLoad[i]).data).flatten(AOut);
            Result := Result + getSeperator(i) + TmusclePoint(TPointDataRec(FPayLoad[i]).data).toString + #13#10;
          end;

        B_RECT_TYPE:
          begin
            //TmuscleRect(TPointDataRec(FPayLoad[i]).data).flatten(AOut);
            Result := Result + getSeperator(i) + TmuscleRect(TRectDataRec(FPayLoad[i]).data).toString + #13#10
          end;

        B_MESSAGE_TYPE:
          begin

            //m := TmuscleMessage(TPortableMessageDataRec(FPayLoad[i]).Data);

            //i32 := m.flattenedSize;
            //AOut.WriteBuffer(i32, sizeof(longint)); //write size

            //m.flatten(AOut); //write actual message...

            Result := Result + getSeperator(i) + #13#10'>>>>>>>>>>>>>>>>'#13#10 + (TPortableMessageDataRec(FPayLoad[i]).data as ImuscleMessage).toString + #13#10'<<<<<<<<<<<<<<<<'#13#10
          end;

        B_STRING_TYPE:
          begin
            //i32 := length(TLongStringBDataRec(FPayLoad[i]).Data) + 1;
            //s := stralloc(i32);
            //try
            //  StrPCopy(s, TLongStringBDataRec(FPayLoad[i]).Data);
            //  s[i32 - 1] := #0;
            //  AOut.WriteBuffer(i32, sizeof(longint)); //write size
            //  AOut.WriteBuffer(s^, i32); //write size
            //finally
            //  StrDispose(s);
            //end;

            Result := Result + getSeperator(i) + TLongStringBDataRec(FPayLoad[i]).Data + #13#10;
          end;

      else
        (* TODO
           byte [][] array = (byte[][]) _payload;
           out.writeInt(_numItems);
           for (int i=0; i<_numItems; i++)
           {
              out.writeInt(array[i].length);
              out.write(array[i]);
           }
        *)
        raise Exception.Create('Unimplemented');

      end;

  except
    raise;
  end;

end;

function TmuscleMessageField.typeCode: cardinal;
begin
  Result := FtypeCode;
end;

procedure TmuscleMessageField.unflatten(ain: TStream; numBytes: integer);
var
  i: integer;
  b: byte;
  i8: shortint;
  i16: smallint;
  i32: longint;
  i64: int64;
  f: single;
  d: double;
  pt: TmusclePoint;
  r: TmuscleRect;
  m: IPortableMessage;
  s: pchar;

  tmp: integer;										//V1_4_4_x

  msgSize: cardinal;

  AflattenedItemSize: integer;
  AnumItems: integer;
begin
  AflattenedItemSize := flattenedItemSize;

  if (AflattenedItemSize > 0) then
    AnumItems := numBytes div AflattenedItemSize
  else
    AnumItems := 1;

  try
    for i := 0 to AnumItems - 1 do
      case FtypeCode of
        B_BOOL_TYPE:
          begin
            ain.ReadBuffer(b, sizeof(byte));

            FPayLoad.Add(TBoolBDataRec.Create(Boolean(b)));
          end;

        B_INT8_TYPE:
          begin
            ain.ReadBuffer(i8, sizeof(shortint));

            FPayLoad.Add(TInt8BDataRec.Create(i8));
          end;

        B_INT16_TYPE:
          begin
            ain.ReadBuffer(i16, sizeof(smallint));

            FPayLoad.Add(TInt16BDataRec.Create(i16));
          end;

        B_INT32_TYPE:
          begin
            ain.ReadBuffer(i32, sizeof(longint));

            FPayLoad.Add(TInt32BDataRec.Create(i32));
          end;

        B_INT64_TYPE:
          begin
            ain.ReadBuffer(i64, sizeof(int64));

            FPayLoad.Add(TLongBDataRec.Create(i64));
          end;

        B_FLOAT_TYPE:
          begin
            ain.ReadBuffer(f, sizeof(single));

            FPayLoad.Add(TFloatBDataRec.Create(f));
          end;

        B_DOUBLE_TYPE:
          begin
            ain.ReadBuffer(d, sizeof(double));

            FPayLoad.Add(TDoubleBDataRec.Create(d));
          end;

        B_POINT_TYPE:
          begin
            pt := TmusclePoint.Create;
            pt.unflatten(ain, pt.flattenedSize);
            FPayLoad.Add(TPointDataRec.Create(pt));
          end;

        B_RECT_TYPE:
          begin
            r := TmuscleRect.Create;
            r.unflatten(ain, r.flattenedSize);
            FPayLoad.Add(TRectDataRec.Create(r));
          end;

        B_MESSAGE_TYPE:
          begin
            msgSize := numBytes;

            while (msgSize > 0) do
            begin
              m := TPortableMessage.Create;

              ain.ReadBuffer(i32, sizeof(longint)); //read size
              m.unflatten(ain, i32); //read actual message...
              FPayLoad.Add(TPortableMessageDataRec.Create(m));

              dec(msgSize, i32 + 4); // 4 is the sizeof the i32 data item pertaining to the message size...
            end;
          end;

        B_STRING_TYPE:
          begin
            //god only knows how this worked...										//V1_4_4_x START

            msgSize := numBytes;

            ain.Read(tmp, sizeof(longint));//number of elements
            dec(msgSize, sizeof(longint)); //i32 + 4);


            while (msgSize > 0) do
            begin
              ain.Read(i32, sizeof(longint));//size of block
              s := StrAlloc(i32);
              try //V1_4_3_9
                tmp := tmp + ain.Read(s^, i32); //write size
                FPayLoad.Add(TLongStringBDataRec.Create(s));

              finally //V1_4_3_9
                StrDispose(s); //V1_4_3_9
              end; //V1_4_3_9

              dec(msgSize, i32 + 4);
            end;
          end;

      else
        (* TODO
           byte [][] array = (byte[][]) _payload;
           out.writeInt(_numItems);
           for (int i=0; i<_numItems; i++)
           {
              out.writeInt(array[i].length);
              out.write(array[i]);
           }
        *)
        raise Exception.Create('Unimplemented');

      end;

  except
    inherited;
  end;
end;

{ TLongBDataRec }

constructor TLongBDataRec.Create(AData: Int64);
begin
  inherited Create(B_INT64_TYPE);
  Data := AData;
end;

constructor TLongBDataRec.CreateStr(AData: string);
begin

end;

function TLongBDataRec.GetData: Variant;
begin
  Result := IntToStr(Data);
end;

procedure TLongBDataRec.SetData(AData: variant);
begin
  Data := StrToInt(AData);
end;

function TLongBDataRec.SizeOf: cardinal;
begin
  result := System.sizeof(int64);
end;

{ TDoubleBDataRec }

constructor TDoubleBDataRec.Create(AData: Double);
begin
  inherited Create(B_DOUBLE_TYPE);

  DATA := AData;
end;

function TDoubleBDataRec.GetData: Variant;
begin
  Result := Data;
end;

procedure TDoubleBDataRec.SetData(AData: variant);
begin
  Data := AData;
end;

function TDoubleBDataRec.SizeOf: cardinal;
begin
  Result := System.sizeof(double);
end;

{ TRectDataRec }

constructor TRectDataRec.Create(AData: TmuscleRect);
begin
  inherited Create(B_RECT_TYPE);

  Data := TmuscleRect.Create(AData);
end;

constructor TRectDataRec.CreateStr(AData: TStream);
begin
  { TODO : ... }
end;

destructor TRectDataRec.Destroy;
begin
  Data.free;

  inherited;
end;

function TRectDataRec.GetData: Variant;
begin
  Result := 0;
end;

procedure TRectDataRec.SetData(AData: variant);
begin
  { TODO : ... }
end;

function TRectDataRec.SizeOf: cardinal;
begin
  Result := Data.flattenedSize;
end;

{ TPointDataRec }

constructor TPointDataRec.Create(AData: TmusclePoint);
begin
  inherited Create(B_POINT_TYPE);

  Data := TmusclePoint.Create(AData.x, AData.y);
end;

constructor TPointDataRec.CreateStr(AData: TStream);
begin
  { TODO : ... }
end;

destructor TPointDataRec.Destroy;
begin
  Data.Free;

  inherited;
end;

function TPointDataRec.GetData: Variant;
begin
  result := 0;
end;

procedure TPointDataRec.SetData(AData: variant);
begin
  { TODO : ... }
end;

function TPointDataRec.SizeOf: cardinal;
begin
  Result := Data.flattenedSize;
end;

{function TBinStreamBDataRec.SizeOf: cardinal;
begin
  Result := Data.Size;
end;}

{ TmuscleMessage }

procedure TmuscleMessage.addBool(const Name: string; const data: boolean);
begin
  inherited;
end;

procedure TmuscleMessage.addDouble(const Name: string; const data: double);
begin
  inherited;

end;

procedure TmuscleMessage.addFloat(const Name: string; const data: single);
begin
  inherited;

end;

procedure TmuscleMessage.addInt16(const Name: string; const data: SmallInt);
begin
  inherited;

end;

procedure TmuscleMessage.addInt32(const Name: string; const data: Integer);
begin
  inherited;

end;

procedure TmuscleMessage.addInt64(const Name: string; const data: int64);
begin
  inherited;

end;

procedure TmuscleMessage.addInt8(const Name: string; const data: Shortint);
begin
  inherited;

end;

procedure TmuscleMessage.addMessage(const Name: string; data: IPortableMessage);
begin
  inherited;

end;

procedure TmuscleMessage.addPoint(const Name: string; data: TmusclePoint);
begin
  inherited;

end;

procedure TmuscleMessage.addRect(const Name: string; data: TmuscleRect);
begin
  inherited;

end;

procedure TmuscleMessage.addString(const Name, data: string);
begin
  inherited;

end;

function TmuscleMessage.findBool(const Name: string; var data: boolean): boolean;
begin
  result := inherited findBool(Name, data);
end;

function TmuscleMessage.findBool(const Name: string; index: integer; var data: boolean): boolean;
begin
  result := inherited findBool(Name, index, data);
end;

function TmuscleMessage.findDouble(const Name: string; index: integer; var data: double): boolean;
begin
  result := inherited findDouble(name, index, data);
end;

function TmuscleMessage.findDouble(const Name: string; var data: double): boolean;
begin
  result := inherited findDouble(name, data);
end;

function TmuscleMessage.findFloat(const Name: string; var data: single): boolean;
begin
  result := inherited findFloat(name, data);
end;

function TmuscleMessage.findFloat(const Name: string; index: integer; var data: single): boolean;
begin
  result := inherited findFloat(name, index, data);
end;

function TmuscleMessage.findInt16(const Name: string; var data: SmallInt): boolean;
begin
  result := inherited findInt16(name, data);
end;

function TmuscleMessage.findInt16(const Name: string; index: integer; var data: SmallInt): boolean;
begin
  result := inherited findInt16(name, index, data);
end;

function TmuscleMessage.findInt32(const Name: string; var data: Integer): boolean;
begin
  result := inherited findInt32(name, data);
end;

function TmuscleMessage.findInt32(const Name: string; index: integer; var data: Integer): boolean;
begin
  result := inherited findInt32(name, index, data);
end;

function TmuscleMessage.findInt64(const Name: string; index: integer; var data: Int64): boolean;
begin
  result := inherited findInt64(name, index, data);
end;

function TmuscleMessage.findInt64(const Name: string; var data: Int64): boolean;
begin
  result := inherited findInt64(name, data);
end;

function TmuscleMessage.findInt8(const Name: string; var data: ShortInt): boolean;
begin
  result := inherited findInt8(name, data);
end;

function TmuscleMessage.findInt8(const Name: string; index: integer; var data: ShortInt): boolean;
begin
  result := inherited findInt8(name, index, data);
end;

function TmuscleMessage.findMessage(const Name: string; index: integer; var data: IPortableMessage): boolean;
begin
  result := inherited findMessage(name, index, data);
end;

function TmuscleMessage.findMessage(const Name: string; var data: IPortableMessage): boolean;
begin
  result := inherited findMessage(name, data);
end;

function TmuscleMessage.findPoint(const Name: string; var data: TmusclePoint): boolean;
begin
  result := inherited findPoint(name, data);
end;

function TmuscleMessage.findPoint(const Name: string; index: integer; var data: TmusclePoint): boolean;
begin
  result := inherited findPoint(name, index, data);
end;

function TmuscleMessage.findRect(const Name: string; var data: TmuscleRect): boolean;
begin
  result := inherited findRect(name, data);
end;

function TmuscleMessage.findRect(const Name: string; index: integer; var data: TmuscleRect): boolean;
begin
  result := inherited findRect(name, index, data);
end;

function TmuscleMessage.findString(const Name: string; index: integer; var data: string): boolean;
begin
  result := inherited findString(name, index, data);
end;

function TmuscleMessage.findString(const Name: string; var data: string): boolean;
begin
  result := inherited findString(name, data);
end;

function TmuscleMessage.replaceBool(const Name: string; const data: boolean): boolean;
begin
  result := inherited replaceBool(name, data);
end;

function TmuscleMessage.replaceDouble(const Name: string; const data: double): boolean;
begin
  result := inherited replaceDouble(name, data);
end;

function TmuscleMessage.replaceFloat(const Name: string; const data: single): boolean;
begin
  result := inherited replaceFloat(name, data);
end;

function TmuscleMessage.replaceInt16(const Name: string; const data: SmallInt): boolean;
begin
  result := inherited replaceInt16(name, data);
end;

function TmuscleMessage.replaceInt32(const Name: string; const data: Integer): boolean;
begin
  result := inherited replaceInt32(name, data);
end;

function TmuscleMessage.replaceInt64(const Name: string; const data: Int64): boolean;
begin
  result := inherited replaceInt64(name, data);
end;

function TmuscleMessage.replaceInt8(const Name: string; const data: ShortInt): boolean;
begin
  result := inherited replaceInt8(name, data);
end;

function TmuscleMessage.replaceMessage(const Name: string; data: IPortableMessage): boolean;
begin
  result := inherited replaceMessage(name, data);
end;

function TmuscleMessage.replaceMessage(const Name: string; index: integer; data: IPortableMessage): boolean;
begin
  result := inherited replaceMessage(name, index, data);
end;

function TmuscleMessage.replacePoint(const Name: string; data: TmusclePoint): boolean;
begin
  result := inherited replacePoint(name, data);
end;

function TmuscleMessage.replaceRect(const Name: string; data: TmuscleRect): boolean;
begin
  result := inherited replaceRect(name, data);
end;

function TmuscleMessage.replaceString(const Name, data: string): boolean;
begin
  result := inherited replaceString(name, data);
end;

{ TPortableMessage }

function TPortableMessage.__Flatten: string;
begin

end;

procedure TPortableMessage.__Unflatten(data: string);
begin

end;

procedure TPortableMessage.AddBoolean(const Name: string; const data: boolean);
begin
  AddBool(name, data);
end;

procedure TPortableMessage.AddCardinal(const Name: string; const data: cardinal);
begin
  AddInteger(Name, integer(data));
end;

procedure TPortableMessage.AddDataStream(const Name: string; data: TStream);
var
  idx: integer;
  fld: TmuscleMessageField;
  b: TBDataRec;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    b := TBLOBDataRec.Create(Data);
    fld.PayLoad.Add(b);
  end
  else
  begin
    b := TBLOBDataRec.Create(Data);
    fld := TmuscleMessageField.Create(b.muscledatatype);
    fld.PayLoad.Add(b);
    _fieldTable.AddObject(name, fld);
  end;
end;

procedure TPortableMessage.AddFloat(const Name: string; const data: single);
begin
  inherited;

end;

procedure TPortableMessage.AddInteger(const Name: string; const data: integer);
begin
  //lets not worry about packing the integer for now...
  addint32(name, data);
end;

function TPortableMessage.FindBoolean(const Name: string): boolean;
begin
  findBool(name, Result);
end;

function TPortableMessage.FindCardinal(const Name: string): Cardinal;
var
  i: integer;
begin
  //we always store this as an int32 and apply signing via type conversion
  findInt32(name, i);

  Result := cardinal(i);
end;

function TPortableMessage.FindDataStream(const Name: string; data: TStream): boolean;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  idx := _fieldTable.IndexOf(name);
  if (idx > -1) then
  begin
    fld := TmuscleMessageField(_fieldTable.Objects[idx]);
    if (fld.Payload.Count > 0) then
    begin
      TBLOBDataRec(fld.Payload[0]).Data.position := 0;
      Data.CopyFrom(TBLOBDataRec(fld.Payload[0]).Data, TBLOBDataRec(fld.Payload[0]).Data.Size);
      Result := True;
      exit;
    end;
  end;

  Result := False;
end;

function TPortableMessage.FindFloat(const Name: string): single;
begin
  findfloat(name, result);
end;

function TPortableMessage.FindInteger(const Name: string): Integer;
begin
  //no packing is implemented for speed, otherwise we'd need to search int8, 16 and 32
  findInt32(name, result);
end;

function TPortableMessage.FindMessage(const Name: string): IPortableMessage;
begin
  findMessage(name, result);
end;

function TPortableMessage.FindString(const Name: string): string;
begin
  findstring(name, result);
end;

procedure TPortableMessage.FindVariant(const Name: string; var data: variant);
begin
  //this is no longer supported.
  raise exception.create('TPortableMessage.FindVariant is unsupported');
end;

function TPortableMessage.GetWhat: Cardinal;
begin
  result := fWhat;
end;

function TPortableMessage.ReplaceBoolean(const Name: string; const data: boolean): boolean;
begin
  result := replacebool(name, data);
end;

function TPortableMessage.ReplaceCardinal(const Name: string; const data: cardinal): boolean;
begin
  result := replaceInteger(name, data);
end;

function TPortableMessage.ReplaceFloat(const Name: string; const data: single): boolean;
begin
  result := inherited ReplaceFloat(name, data);
end;

function TPortableMessage.ReplaceInteger(const Name: string; const data: integer): boolean;
begin
  result := replaceInt32(name, data);
end;

function TPortableMessage.ReplaceMessage(const Name: string; data: IPortableMessage): boolean;
begin
  //todo

  result := inherited ReplaceMessage(name, data);
end;

function TPortableMessage.ReplaceString(const Name, data: string): boolean;
begin
  result := inherited ReplaceString(name, data);
end;

procedure TPortableMessage.SetWhat(const Value: Cardinal);
begin
  Fwhat := Value;
end;

procedure TPortableMessage.AddMessage(const Name: string; data: IPortableMessage);
begin
  inherited;
end;

procedure TPortableMessage.AddString(const Name, data: string);
begin
  inherited;

end;

//------------------------------------------------------------------------------------------------------------------------------------------

function TPortableMessage.FindDataStreamData(const Name: string; toStream: TStream; dataCount: integer): integer;
var
  idx: integer;
  fld: TmuscleMessageField;
begin
  Result := 0;

  if Assigned(toStream) then
  begin
    idx := _fieldTable.IndexOf(name);
    if (idx > -1) then
    begin
      fld := TmuscleMessageField(_fieldTable.Objects[idx]);
      if (fld.Payload.Count > 0) then
      begin
        TBLOBDataRec(fld.Payload[0]).Data.position := 0;

        if (TBLOBDataRec(fld.Payload[0]).Data.Size < dataCount) then
        begin
          toStream.CopyFrom(TBLOBDataRec(fld.Payload[0]).Data, TBLOBDataRec(fld.Payload[0]).Data.Size);
          Result := TBLOBDataRec(fld.Payload[0]).Data.Size;
        end
        else
        begin
          toStream.CopyFrom(TBLOBDataRec(fld.Payload[0]).Data, dataCount);
          Result := dataCount;
        end;

        exit;
      end;
    end;
  end;
end;

procedure TPortableMessage.AddDouble(const Name: string; const data: double);					//new fn V1_4_4_x
begin
  inherited;

end;

function TPortableMessage.ReplaceDouble(const Name: string; const data: double): boolean;				//new fn V1_4_4_x
begin
  result := inherited ReplaceDouble(name, data);
end;

function TPortableMessage.FindDouble(const Name: string): double;						//new fn V1_4_4_x
begin
  finddouble(name, result);
end;

procedure TPortableMessage.copyFrom(const source: IPortableMessage);
var
  ms: TMemoryStream;
begin
  ms := TMemoryStream.Create;
  try
    source.flatten(ms);
    ms.Seek(0, soFromBeginning);
    self.unflatten(ms, ms.size);
  finally
    ms.free;
  end;
end;

{ TBLOBDataRec }

procedure TBLOBDataRec.DataAsStream(s: TStream);
begin
  data.position := 0;
  s.copyfrom(data, data.size);
end;

constructor TBLOBDataRec.Create(AData: TStream);
begin
  inherited Create(B_RAW_TYPE);

  data := TMemoryStream.Create;

  AData.position := 0;
  data.CopyFrom(AData, AData.Size);
end;

constructor TBLOBDataRec.CreateFromHex(AData: string);
var
  s: TStringStream;
  ms: TMemoryStream;
begin
  s := TStringStream.Create(AData);
  ms := TMemoryStream.Create;
  try
    s.position := 0;

    StreamHexToBin(s, ms);
    ms.position := 0;

    Create(ms);
  finally
    ms.free;
    s.free;
  end;
end;

function TBLOBDataRec.DataAsString: string;
var
  s: TStringStream;
begin

  s := TStringStream.Create('');
  try
    data.position := 0;
    StreamBinToHex(data, s);

    Result := s.Datastring;
  finally
    s.free;
  end;
end;

destructor TBLOBDataRec.Destroy;
begin
  data.free;

  inherited;
end;

function TBLOBDataRec.GetData: Variant;
begin
  //
  result := null;
end;

procedure TBLOBDataRec.SetData(AData: variant);
begin
  /// not implemented...
end;

function TBLOBDataRec.SizeOf: cardinal;
begin
  Result := Data.Size;
end;

end.

