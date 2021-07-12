{-----------------------------------------------------------------------------
 Unit Name: TypeConstants
 Author:    Matthew Emson
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



unit TypeConstants;

interface

//** declarations for Be's type constants.  The values are all the same as Be's. */

const //all 'integer'
  B_ANY_TYPE     = 1095653716; // 'ANYT',  // wild card
  B_BOOL_TYPE    = 1112493900; // 'BOOL',
  B_DOUBLE_TYPE  = 1145195589; // 'DBLE',
  B_FLOAT_TYPE   = 1179406164; // 'FLOT',
  B_INT64_TYPE   = 1280069191; // 'LLNG',  // a.k.a. long in Java
  B_INT32_TYPE   = 1280265799; // 'LONG',  // a.k.a. int in Java
  B_INT16_TYPE   = 1397248596; // 'SHRT',  // a.k.a. short in Java
  B_INT8_TYPE    = 1113150533; // 'BYTE',  // a.k.a. byte in Java
  B_MESSAGE_TYPE = 1297303367; // 'MSGG',
  B_POINTER_TYPE = 1347310674; // 'PNTR',  // parsed as int in Java (but not very useful)
  B_POINT_TYPE   = 1112559188; // 'BPNT',  // muscle.support.Point in Java
  B_RECT_TYPE    = 1380270932; // 'RECT',  // muscle.support.Rect in Java
  B_STRING_TYPE  = 1129534546; // 'CSTR',  // java.lang.String
  B_OBJECT_TYPE  = 1330664530; // 'OPTR',
  B_RAW_TYPE     = 1380013908; // 'RAWT',
  B_MIME_TYPE    = 1296649541; // 'MIME',

  B_TAG_TYPE     = 1297367367;  //* 'MTAG' = new for v2.00; for in-mem-only tags         */

  B_ERROR = -1;
  B_NO_ERROR = 0;
  B_OK = B_NO_ERROR;

type
  uint32 = longword;
  int32 = longint;
  int16 = smallint;
  uint16 = word;
  int8 = shortint;
  uint6 = int64; //not much else we can do!!

  float = single; //just to make code similar..

  status_t = integer;


function {MuscleX86SwapDouble} B_SWAP_DOUBLE(val: double): double; cdecl;
function {MuscleX86SwapFloat}  B_SWAP_FLOAT(val: float): float; cdecl;
function {MuscleX86SwapInt64}  B_SWAP_INT64(val: int64): int64; cdecl;
function {MuscleX86SwapInt32}  B_SWAP_INT32(val: uint32): uint32; cdecl;
function {MuscleX86SwapInt16}  B_SWAP_INT16(val: uint16): uint16; cdecl;

procedure UnitTest;  //test stuff, including the byteswapping routines..

implementation


function {MuscleX86SwapInt16} B_SWAP_INT16(val: uint16): uint16; cdecl;
begin
  asm
    mov ax, val;
    xchg al, ah;
    mov val, ax;
  end;
  result := val;
end;


function {MuscleX86SwapInt32} B_SWAP_INT32(val: uint32): uint32; cdecl;
begin
  asm
    mov eax, val;
    bswap eax;
    mov val, eax;
  end;
  result := val;
end;

function {MuscleX86SwapFloat} B_SWAP_FLOAT(val: float): float; cdecl;
begin
  asm
    mov eax, val;
    bswap eax;
    mov val, eax;
  end;

  result := val;
end;

function {MuscleX86SwapInt64} B_SWAP_INT64(val: int64): int64; cdecl;
begin
  asm
    mov eax, DWORD PTR val;
    mov edx, DWORD PTR val + 4;
    bswap eax;
    bswap edx;
    mov DWORD PTR val, edx;
    mov DWORD PTR val + 4, eax;
  end;
  result := val;
end;

function {MuscleX86SwapDouble} B_SWAP_DOUBLE(val: double): double; cdecl;
begin
  asm
    mov eax, DWORD PTR val;
    mov edx, DWORD PTR val + 4;
    bswap eax;
    bswap edx;
    mov DWORD PTR val, edx;
    mov DWORD PTR val + 4, eax;
  end;
  result := val;
end;


{-----------------------------------------------------------------------------
  Procedure: unittest
  Author:    Matthew Emson
  Purpose:   test stuff, including the byteswapping routines..
-----------------------------------------------------------------------------}
procedure unittest;
var
  a, f: int16;
  b, g: int32;
  c, h: int64;
  d, i, k: float;
  e, j, l: double;

begin
  a := 12345;
  f := B_SWAP_INT16(a);
  a := B_SWAP_INT16(f);

  assert(a = 12345);

  b := 1234567;
  g := B_SWAP_INT32(b);
  b := B_SWAP_INT32(g);

  assert(b = 1234567);

  c := 1234567891011;
  h := B_SWAP_INT64(c);
  c := B_SWAP_INT64(h);

  assert(c = 1234567891011);

  d := 123.456;
  i := B_SWAP_FLOAT(d);
  d := B_SWAP_FLOAT(i) *1000;
  k := Round(d); //singles and rounding suck..

  assert(k = 123456);

  e := 12345.6789;
  j := B_SWAP_DOUBLE(e);
  e := B_SWAP_DOUBLE(j) * 10000;
  l := Round(e); //doubles and rounding suck..

  assert(l = 123456789);
end;

end.
