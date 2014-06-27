{-----------------------------------------------------------------------------
 Unit Name: Rect
 Author:    mathew emson
 Date:      30-Oct-2003
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



unit Rect;

interface

uses
  Classes, SysUtils,

  TypeConstants,
  Flattenable,
  Point;

type
  TmuscleRect = class(TmuscleFlattenable)
  private
    Fbottom: single;
    Fleft: single;
    Fright: single;
    Ftop: single;
    function getleftBottom: TmusclePoint;
    function getleftTop: TmusclePoint;
    function getrightBottom: TmusclePoint;
    function getrightTop: TmusclePoint;
    procedure setleftBottom(const Value: TmusclePoint);
    procedure setleftTop(const Value: TmusclePoint);
    procedure setrightBottom(const Value: TmusclePoint);
    procedure setrightTop(const Value: TmusclePoint);

  protected
    procedure Setbottom(const Value: single);
    procedure Setleft(const Value: single);
    procedure Setright(const Value: single);
    procedure Settop(const Value: single);
    function getHeight: single;
    function getWidth: single;

  public
    constructor Create; overload; virtual;
    constructor Create(l, t, r, b: single); overload; virtual;
    constructor Create(rhs: TmuscleRect); overload; virtual;
    constructor Create(topLeft, bottomRight: TmusclePoint); overload; virtual;

    property top: single read Ftop write Settop;
    property left: single read Fleft write Setleft;
    property right: single read Fright write Setright;
    property bottom: single read Fbottom write Setbottom;

    //** Set a new position for the rectangle. */ was called 'set'
    procedure setBounds(l, t, r, b: single);                                    

    function toString: string; override;

    //from TmuscleFlattenable
    procedure setEqualTo(setFromMe: TmuscleFlattenable); override;
    function cloneFlat: TmuscleFlattenable; override;

    //Makes the rectangle smaller by the amount specified in both the x and y dimensions
    procedure insetBy(point: TmusclePoint); overload;
    procedure insetBy(dx, dy: single); overload;

    //Translates the rectangle by the amount specified in both the x and y dimensions
    procedure offsetBy(point: TmusclePoint); overload;
    procedure offsetBy(dx, dy: single); overload;

    //Comparison Operator.  Returns true iff (r)'s dimensions are exactly the same as this rectangle's.
    function equals(o: TObject): boolean;

    //Returns a rectangle whose area is the intersecting subset of this rectangle's and (r)'s
    function intersect(r: TmuscleRect): TmuscleRect;

    //Returns true iff this rectangle and (r) overlap in space.
    function intersects(r: TmuscleRect): boolean;

    //Returns true iff this rectangle's area is non imaginary (i.e. width() and height()) are both non-negative)
    function isValid: boolean;

    //Returns a rectangle whose area is a superset of the union of this rectangle's and (r)'s
    function unify(r: TmuscleRect): TmuscleRect;

    //Returns true iff this rectangle contains the specified point.

    function contains( point: TmusclePoint ): boolean; overload;

    function contains( r: TmuscleRect ): boolean; overload;


    //from TmuscleFlattenable
    function isFixedSize: boolean; override;

    function typeCode: cardinal; override;

    function flattenedSize: integer; override;

    //Returns true only if code is B_RECT_TYPE
    function allowsTypeCode(code: integer): boolean; override;

    procedure flatten( AOut: TStream ); override;
    procedure unflatten(ain: TStream; numBytes: integer); override;


    property Height: single read getHeight;
    property Width: single read getWidth;

    property leftTop: TmusclePoint read getleftTop write setleftTop;
    property rightBottom: TmusclePoint read getrightBottom write setrightBottom;
    property rightTop: TmusclePoint read getrightTop write setrightTop;
    property leftBottom: TmusclePoint read getleftBottom write setleftBottom;

  end;

implementation

{ TmuscleRect }

constructor TmuscleRect.Create;
begin
  Fbottom := -1.0;
  Ftop := 0.0;
  Fleft := 0.0;
  Fright := -1.0;
end;

constructor TmuscleRect.Create(l, t, r, b: single);
begin
  setBounds(l, t, r, b);
end;

constructor TmuscleRect.Create(rhs: TmuscleRect);
begin
  setEqualTo(rhs);
end;

function TmuscleRect.cloneFlat: TmuscleFlattenable;
begin
  Result := TmuscleRect.Create(Fleft, Ftop, FRight, Fbottom);
end;

constructor TmuscleRect.Create(topLeft, bottomRight: TmusclePoint);
begin
  setBounds(leftTop.x, leftTop.y, rightBottom.x, rightBottom.y);
end;

procedure TmuscleRect.Setbottom(const Value: single);
begin
  Fbottom := Value;
end;

procedure TmuscleRect.setBounds(l, t, r, b: single);
begin
  Fbottom := b;
  Ftop := t;
  Fleft := l;
  Fright := r;
end;

procedure TmuscleRect.setEqualTo(setFromMe: TmuscleFlattenable);
var
  copyMe: TmuscleRect;
begin
  inherited;

  copyMe := (setFromMe as TmuscleRect);

  setBounds(copyMe.left, copyMe.top, copyMe.right, copyMe.bottom);
end;

procedure TmuscleRect.Setleft(const Value: single);
begin
  Fleft := Value;
end;

procedure TmuscleRect.Setright(const Value: single);
begin
  Fright := Value;
end;

procedure TmuscleRect.Settop(const Value: single);
begin
  Ftop := Value;
end;

function TmuscleRect.getHeight: single;
begin
  Result := Fright - Fleft;
end;

function TmuscleRect.getWidth: single;
begin
  Result := Fbottom - Ftop;
end;

function TmuscleRect.toString: string;
begin
  Result := format('Rect: leftTop=(%f,%f) rightBottom=(%f,%f)', [Fleft, Ftop, Fright, Fbottom]);
end;

function TmuscleRect.getleftBottom: TmusclePoint;
begin
  Result := TmusclePoint.Create(Fleft, Fbottom);
end;

function TmuscleRect.getleftTop: TmusclePoint;
begin
  Result := TmusclePoint.Create(Fleft, Ftop);
end;

function TmuscleRect.getrightBottom: TmusclePoint;
begin
  Result := TmusclePoint.Create(Fright, Fbottom);
end;

function TmuscleRect.getrightTop: TmusclePoint;
begin
  Result := TmusclePoint.Create(Fright, Ftop);
end;

procedure TmuscleRect.setleftBottom(const Value: TmusclePoint);
begin
  Fleft := Value.x;
  Fbottom := Value.y;
end;

procedure TmuscleRect.setleftTop(const Value: TmusclePoint);
begin
  Fleft := Value.x;
  Ftop := Value.y;
end;

procedure TmuscleRect.setrightBottom(const Value: TmusclePoint);
begin
  Fright := Value.x;
  Fbottom := Value.y;
end;

procedure TmuscleRect.setrightTop(const Value: TmusclePoint);
begin
  Fright := Value.x;
  Ftop := Value.y;
end;

procedure TmuscleRect.insetBy(point: TmusclePoint);
begin
  insetBy(point.x, point.y);
end;

procedure TmuscleRect.insetBy(dx, dy: single);
begin
  Fleft := Fleft + dx;
  Ftop := Ftop + dy;
  Fright := Fright - dx;
  Fbottom := Fbottom - dy;
end;

procedure TmuscleRect.offsetBy(point: TmusclePoint);
begin
  offsetBy(point.x, point.y);
end;

procedure TmuscleRect.offsetBy(dx, dy: single);
begin
  {left += dx; top += dy; right += dx; bottom += dy}

  Fleft := Fleft + dx;
  Ftop := Ftop + dy;
  Fright := Fright + dx;
  Fbottom := Fbottom + dy;
end;

function TmuscleRect.equals(o: TObject): boolean;
var
  r: TmuscleRect;
begin
  if (o is TmuscleRect) then begin
    r := TmuscleRect(o);
    Result := (Fleft = r.left) and (Ftop = r.top) and (Fright = r.right) and (Fbottom = r.bottom);
  end
  else
    Result := false;
end;

function TmuscleRect.intersect(r: TmuscleRect): TmuscleRect;
begin
  Result := TmuscleRect.Create(Self);

  if (Result.left < r.left) then
    Result.left := r.left;

  if (Result.right > r.right) then
    Result.right := r.right;

  if (Result.top < r.top) then
    Result.top := r.top;

  if (Result.bottom > r.bottom) then
    Result.bottom := r.bottom;

end;

function TmuscleRect.unify(r: TmuscleRect): TmuscleRect;
begin
  Result := TmuscleRect.Create(Self);

  if (r.left < Result.left) then
    Result.left := r.left;

  if (r.right > Result.right) then
    Result.right := r.right;

  if (r.top < Result.top) then
    Result.top := r.top;

  if (r.bottom > Result.bottom) then
    Result.bottom := r.bottom;
end;

function TmuscleRect.intersects(r: TmuscleRect): boolean;
begin
  Result := r.intersect(self).isValid;
end;

function TmuscleRect.isValid: boolean;
begin
  {return ((width() >= 0.0f)&&(height() >= 0.0f));}
  Result := (width >= 0.0) and (height >= 0.0);
end;

function TmuscleRect.contains(point: TmusclePoint): boolean;
begin
  Result :=  ((point.x >= Fleft) and (point.x <= Fright) and (point.y >= Ftop) and (point.y <= Fbottom));
end;

function TmuscleRect.contains(r: TmuscleRect): boolean;
begin
  Result :=  ((contains(r.leftTop)) and (contains(r.rightTop)) and
              (contains(r.leftBottom)) and (contains(r.rightBottom)));
end;

function TmuscleRect.isFixedSize: boolean;
begin
  Result := true;
end;

function TmuscleRect.typeCode: cardinal;
begin
  Result := TypeConstants.B_RECT_TYPE; // 'RECT';
end;

function TmuscleRect.flattenedSize: integer;
begin
  Result := SizeOf(Single) * 4; //16...
end;

function TmuscleRect.allowsTypeCode(code: integer): boolean;
begin
  Result := (code = TypeConstants.B_RECT_TYPE);
end;

procedure TmuscleRect.flatten(AOut: TStream);
begin
  try
    AOut.Write(Fleft,   SizeOf(Single));
    AOut.Write(Ftop,    SizeOf(Single));
    AOut.Write(Fright,  SizeOf(Single));
    AOut.Write(Fbottom, SizeOf(Single));
  except
    inherited;
  end;
end;

procedure TmuscleRect.unflatten(ain: TStream; numBytes: integer);
begin
  try
    ain.Read(Fleft,   SizeOf(Single));
    ain.Read(Ftop,    SizeOf(Single));
    ain.Read(Fright,  SizeOf(Single));
    ain.Read(Fbottom, SizeOf(Single));
  except
    inherited;
  end;
end;

end.
