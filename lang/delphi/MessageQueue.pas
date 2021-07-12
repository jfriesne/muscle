{-----------------------------------------------------------------------------
 Unit Name: MessageQueue
 Author:    matt
 Date:      17-Sep-2004
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



unit MessageQueue;

interface


uses
  Sysutils, Classes, Windows, Messages, Message, Locker, Contnrs;

type

  //The queue is FIFO
  TMessageQueue = class
  private
    private_list: TInterfaceList;
    locker: TInterfaceListLocker; //because Borland (and the VCL) are a not MT Friendly, unless I use
                                  //this, many of the TInterfaceList methods actually use a Critical
                                  //Section that blocks till it's released!

  public
    constructor Create;
    destructor  Destroy; override;

    function push(const data: IPortableMessage): boolean;
    function pop(out queueCount: cardinal): IPortableMessage;
  end;


implementation

//------------------------------------------------------------------------------

{ TMessageQueue }

constructor TMessageQueue.Create;
begin
  private_list := TInterfaceList.Create;
  locker := TInterfaceListLocker.Create(nil);
  locker.InterfaceList := private_list;
end;

//------------------------------------------------------------------------------

destructor TMessageQueue.Destroy;
begin
  locker.Free;
  private_list.Free;

  inherited;
end;

//------------------------------------------------------------------------------

function TMessageQueue.pop(out queueCount: cardinal): IPortableMessage;
var
  list: TInterfaceList;
begin
  // for this we will attempt to lock for 2 seconds
  list := locker.LockEx(2000);
  if assigned(list) then
  try
    if (list.count > 0) then
    begin
      result := (list.first as IPortableMessage);
      list.delete(0); //aarg!! this locks the list!!
    end
    else
      result := nil;

    queueCount := list.count;
  finally
    locker.Unlock;
  end;
end;

//------------------------------------------------------------------------------

function TMessageQueue.push(const data: IPortableMessage): boolean;
var
  list: TInterfaceList;
begin
  result := false;
  try
  // for this we will attempt to lock for 2 seconds
  list := locker.LockEx(2000);

  result := assigned(list);
  if result then
  try
    list.add(data);
  finally
    locker.Unlock;
  end;
  except
    on e: exception do 
    MessageBox(0, pchar(e.message), '', 0);
  end;
end;

end.
