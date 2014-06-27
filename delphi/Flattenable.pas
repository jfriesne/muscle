{-----------------------------------------------------------------------------
 Unit Name: Flattenable
 Author:    Matthew Emson
 Date:      30-Oct-2003
 Purpose:   Interfaced TPersistent alike
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




unit Flattenable;

interface

uses
  Classes, Sysutils;

//** Interface for objects that can be flattened and unflattened from Be-style byte streams */
type
  TmuscleFlattenable = class(TInterfacedObject)
  public
    //** Should return true iff every object of this type has a flattened size that is known at compile time. */
    function isFixedSize: boolean; virtual; abstract;

    //** Should return the type code identifying this type of object.  */
    function typeCode: cardinal; virtual; abstract;

    //** Should return the number of bytes needed to store this object in its current state.  */
    function flattenedSize: integer; virtual; abstract;

    //** Should return a clone of this object */
    function cloneFlat: TmuscleFlattenable; virtual;

    //** Should set this object's state equal to that of (setFromMe), or throw an UnflattenFormatException if it can't be done.
    //*  @param setFromMe The object we want to be like.
    //*  @throws ClassCastException if (setFromMe) is the wrong type.
    //*/
    procedure setEqualTo( setFromMe: TmuscleFlattenable ); virtual;  //throws ClassCastException;  ... assign???

    //**
    //*  Should store this object's state into (buffer).
    //*  @param out The DataOutput to send the data to.
    //*  @throws IOException if there is a problem writing out the output bytes.
    //*/
    procedure flatten( AOut: TStream ); virtual;  // (DataOutput out) throws IOException;

    //**
    //*  Should return true iff a buffer with type_code (code) can be used to reconstruct
    //*  this object's state.
    //*  @param code A type code ant, e.g. B_RAW_TYPE or B_STRING_TYPE, or something custom.
    //*  @return True iff this object can Unflatten from a buffer of the given type, false otherwise.
    //*/
    function allowsTypeCode(code: integer): boolean; virtual;

    //**
    //*  Should attempt to restore this object's state from the given buffer.
    //*  @param in The stream to read the object from.
    //*  @param numBytes The number of bytes the object takes up in the stream, or negative if this is unknown.
    //*  @throws IOException if there is a problem reading in the input bytes.
    //*  @throws UnflattenFormatException if the bytes that were read in weren't in the expected format.
    //*/
    procedure unflatten(ain: TStream; numBytes: integer); virtual;// throws IOException, UnflattenFormatException;


    //mainly for testing...
    function toString: string; virtual; abstract;
    
end;

implementation

uses
  MuscleExceptions;

{ TmuscleFlattenable }

function TmuscleFlattenable.allowsTypeCode(code: integer): boolean;
begin
  result := false;
end;

function TmuscleFlattenable.cloneFlat: TmuscleFlattenable;
begin
  result := nil;
end;

procedure TmuscleFlattenable.flatten(AOut: TStream);
begin
  raise EInOutError.Create('Unable to flatten');
end;

procedure TmuscleFlattenable.setEqualTo(setFromMe: TmuscleFlattenable);
begin
  if (setFromMe.ClassName <> Self.ClassName) then
    raise EInvalidCast.Create('bad cast');
end;

procedure TmuscleFlattenable.unflatten(ain: TStream; numBytes: integer);
begin
  raise EUnflattenFormatException.CreateDefault;
end;

end.
