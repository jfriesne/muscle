{-----------------------------------------------------------------------------
  Procedure: Point
  Author:    Matthew Emson
  Date:      30-Oct-2003
  Purpose:
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



unit Point;

interface

uses
  Classes, SysUtils,

  TypeConstants,
  Flattenable;


type
  TmusclePoint = class(TmuscleFlattenable)
  private
    Fy: single;
    Fx: single;

    procedure Setx(const Value: single);
    procedure Sety(const Value: single);

  public
    constructor Create; overload; virtual;
    constructor Create( ax, ay: single ); overload; virtual;

    procedure setBounds( dx, dy: single );

    procedure constrainTo( topLeft, bottomRight: TmusclePoint );
    function add(p: TmusclePoint): TmusclePoint;
    function subtract(p: TmusclePoint): TmusclePoint;

    procedure addToThis(p: TmusclePoint);
    procedure subtractFromThis(p: TmusclePoint);

    //Comparison Operator.  Returns true iff (r)'s dimensions are exactly the same as this rectangle's.
    function equals(o: TObject): boolean;


    //from TmuscleFlattenable
    function isFixedSize: boolean; override;
    function typeCode: cardinal; override;
    function flattenedSize: integer; override;
    function cloneFlat: TmuscleFlattenable; override;
    procedure setEqualTo( setFromMe: TmuscleFlattenable ); override;

    procedure flatten( AOut: TStream ); override;
    function allowsTypeCode(code: integer): boolean; override;
    procedure unflatten(ain: TStream; numBytes: integer); override;// throws IOException, UnflattenFormatException;

    //mainly for testing...
    function toString: string; override;

    property x: single read Fx write Setx;
    property y: single read Fy write Sety;
  end;


implementation

uses
  Rect;

{ TmusclePoint }

function TmusclePoint.add(p: TmusclePoint): TmusclePoint;
begin
  Result := TmusclePoint.Create( Fx + p.x, Fy + p.y);
end;

procedure TmusclePoint.addToThis(p: TmusclePoint);
begin
  Fx := Fx + p.x;
  Fy := Fy + p.y;
end;

function TmusclePoint.allowsTypeCode(code: integer): boolean;
begin
  Result := (code = TypeConstants.B_POINT_TYPE);
end;

function TmusclePoint.cloneFlat: TmuscleFlattenable;
begin
  Result := TmusclePoint.Create( Fx, Fy );
end;

procedure TmusclePoint.constrainTo(topLeft, bottomRight: TmusclePoint);
begin
  if (Fx < topLeft.x) then
    Fx := topLeft.x;

  if (Fy < topLeft.y) then
    Fy := topLeft.y;

  if (Fx > bottomRight.x) then
    Fx := bottomRight.x;

  if (Fy > bottomRight.y) then
    Fy := bottomRight.y;
end;

constructor TmusclePoint.Create(ax, ay: single);
begin

end;

constructor TmusclePoint.Create;
begin
  Fx := 0.0;
  Fy := 0.0;
end;

function TmusclePoint.equals(o: TObject): boolean;
var
  p: TmusclePoint;
begin
  if (o is TmuscleRect) then begin
    p := TmusclePoint(o);
    Result := (Fx = p.x) and (Fy = p.y);
  end
  else
    Result := false;

end;

procedure TmusclePoint.flatten(AOut: TStream);
begin
  try
    AOut.Write(Fx, SizeOf(Single));
    AOut.Write(Fy, SizeOf(Single));
  except
    inherited;
  end;
end;

function TmusclePoint.flattenedSize: integer;
begin
  Result := ( SizeOf(Single) * 2 );
end;

function TmusclePoint.isFixedSize: boolean;
begin
  Result := True;
end;

procedure TmusclePoint.setBounds(dx, dy: single);
begin
  Fx := dx;
  Fy := dy;
end;

procedure TmusclePoint.setEqualTo(setFromMe: TmuscleFlattenable);
var
  copyMe: TmusclePoint;
begin
  inherited;

  copyMe := (setFromMe as TmusclePoint);

  setBounds(copyMe.x, copyMe.y);
end;

procedure TmusclePoint.Setx(const Value: single);
begin
  Fx := Value;
end;

procedure TmusclePoint.Sety(const Value: single);
begin
  Fy := Value;
end;

function TmusclePoint.subtract(p: TmusclePoint): TmusclePoint;
begin
  Result := TmusclePoint.Create( Fx - p.x, Fy - p.y);
end;

procedure TmusclePoint.subtractFromThis(p: TmusclePoint);
begin
  Fx := Fx - p.x;
  Fy := Fy - p.y;
end;

function TmusclePoint.toString: string;
begin
  Result := format('Point: %f %f', [Fx, Fy]);
end;

function TmusclePoint.typeCode: cardinal;
begin
  Result := TypeConstants.B_POINT_TYPE;
end;

procedure TmusclePoint.unflatten(ain: TStream; numBytes: integer);
begin
  try
    ain.Read(Fx, SizeOf(Single));
    ain.Read(Fy, SizeOf(Single));
  except
    inherited;
  end;
end;

end.
