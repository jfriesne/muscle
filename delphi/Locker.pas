{-----------------------------------------------------------------------------
 Unit Name: Locker
 Author:    Matthew Emson
 Date:      ??-??-2003
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




unit Locker;

{ The locker is an idea that grew out of the extreme inability of Borland to understand good and sound methods
  for allowing multiple threads in their application framework to access the same Class using some kind of
  synchronization methodology. The locker hierarchy tries to give less painfull wrappers for various
  classes (including the main base class and base component class) to help the developer cater for a simple
  syncronization mechanism of otherwise asynchronous threads.

  Remember.. this is all or nothing. If you use a locker, you must always access the protected class via the
  locker. Not doing so will seriously mess up syncronization and will probably cause spurious a/v's due
  to nasty things happening in one or more threads.

  By default the lock will timeout after 1 second (1000 miliseconds.) Remember to set an appropriate
  lock timeout for your purposes. 'INFINITE' is possible, but beware of blocking..this may cause
  deadlocks and is a pain to debug. }


interface

uses
  Classes, SysUtils, Semaphore, contnrs, forms;

type
  ELockerError = class(Exception);

  TBaseLocker = class(TComponent)
  private
    FSem: TNamedSemaphore;
    FLockTimeout: cardinal;
    function getSem: TSemaphore;

  protected
    function GetSemaphoreHandle: cardinal;
    function GetIsLocked: boolean;

    procedure SetLockTimeout(const Value: cardinal);

    property IsLocked: boolean read GetIsLocked;
    property LockTimeout: cardinal read FLockTimeout write SetLockTimeout;
    property Sem: TSemaphore read getSem;

  public
    constructor Create(AOwner: TComponent); override;
    destructor Destroy; override;

    procedure Unlock; virtual;

    property SemaphoreHandle: cardinal read GetSemaphoreHandle;
  end;

//------------------------------------------------------------------------------------------------------------------------------------------

  TBaseComponentLocker = class(TBaseLocker)
  private
    FComponent: TComponent;

  protected
    procedure SetComponent(const Value: TComponent);
    property Component: TComponent read FComponent write SetComponent;
    procedure Notification(AComponent: TComponent; Operation: TOperation); override;

  end;

//------------------------------------------------------------------------------------------------------------------------------------------

  TBaseObjectLocker = class(TBaseLocker)
  private
    FSubject: TObject;

  protected
    procedure Notification(AComponent: TComponent; Operation: TOperation); override;

    procedure SetSubject(const Value: TObject);

    property Subject: TObject read FSubject write SetSubject; //object is a reserved word...

    function DoLock( var AObject: TObject; useTimeout: boolean = false; ATimeout: cardinal = 1000 ): boolean; virtual;
  end;

//------------------------------------------------------------------------------------------------------------------------------------------

  TCustomComponentLocker = class(TBaseComponentLocker)
  protected
    function Lock: TComponent; overload; virtual;
    function Lock( var AComponent: TComponent ): boolean; overload; virtual;
  end;

//------------------------------------------------------------------------------------------------------------------------------------------

  TComponentLocker = class(TCustomComponentLocker)
  public
    function Lock: TComponent; override;
    property IsLocked;

  published
    property LockTimeout;
    property Component;

  end;

//------------------------------------------------------------------------------------------------------------------------------------------

  TFormLocker = class(TCustomComponentLocker)
  protected
    function  GetForm: TForm;
    procedure SetForm(const Value: TForm);

  public
    function Lock: TForm; reintroduce; virtual;
    property IsLocked;

  published
    property LockTimeout;
    property Form: TForm read GetForm write SetForm;
  end;

//------------------------------------------------------------------------------------------------------------------------------------------

  TFrameLocker = class(TCustomComponentLocker)
  protected
    function  GetFrame: TFrame;
    procedure SetFrame(const Value: TFrame);

  public
    function Lock: TFrame; reintroduce; virtual;
    property IsLocked;

  published
    property LockTimeout;
    property Frame: TFrame read GetFrame write SetFrame;
  end;

//------------------------------------------------------------------------------------------------------------------------------------------

  TCustomListLocker = class(TBaseObjectLocker)
  protected
    function GetList: TList;
    procedure SetList(const Value: TList);
    function Lock: TList; virtual;
    property List: TList read GetList write SetList;
  end;

//------------------------------------------------------------------------------------------------------------------------------------------

  TListLocker = class(TCustomListLocker)
  public
    function Lock: TList; override;
    function LockEx(const ATimeout: cardinal): TList; virtual;
    property IsLocked;

  published
    property LockTimeout;

    property List;
  end;

//------------------------------------------------------------------------------------------------------------------------------------------

  TObjectListLocker = class(TCustomListLocker)
  private
    function GetObjectList: TObjectList;
    procedure SetObjectList(const Value: TObjectList);

  public
    function Lock: TObjectList; reintroduce; virtual;
    function LockEx(ATimeout: integer): TObjectList;
    property IsLocked;

  published
    property LockTimeout;

    property ObjectList: TObjectList read GetObjectList write SetObjectList;

  end;

//------------------------------------------------------------------------------------------------------------------------------------------


  TOrderedListLocker = class(TBaseObjectLocker)
  private
    function GetOrderedList: TOrderedList;
    procedure SetOrderedList(const Value: TOrderedList);
  public
    function Lock: TOrderedList; reintroduce; virtual;
    property IsLocked;

  published
    property LockTimeout;

    property OrderedList: TOrderedList read GetOrderedList write SetOrderedList;
  end;

//------------------------------------------------------------------------------------------------------------------------------------------

  //TInterfaceList inherites from TThreadList, but htis uses a Critical section to lock... We don't want to block when locking
  TInterfaceListLocker = class(TCustomListLocker)
  private
    function GetInterfaceList: TInterfaceList;
    procedure SetInterfaceList(const Value: TInterfaceList);

  public
    function Lock: TInterfaceList; reintroduce; virtual;
    function LockEx(ATimeout: integer): TInterfaceList;
    property IsLocked;

  published
    property LockTimeout;

    property InterfaceList: TInterfaceList read GetInterfaceList write SetInterfaceList;

  end;

//------------------------------------------------------------------------------------------------------------------------------------------

  TStringsLocker = class(TBaseObjectLocker)
  protected
    function GetStrings: TStrings;
    procedure SetStrings(const Value: TStrings);

  public
    function Lock: TStrings; virtual;
    property IsLocked;

  published
    property LockTimeout;

    property Strings: TStrings read GetStrings write SetStrings;
  end;

//------------------------------------------------------------------------------------------------------------------------------------------


  TStreamLocker = class(TBaseObjectLocker)
  private
    function getStream: TStream;
    procedure setStream(const Value: TStream);

  public
    function Lock: TStream; virtual;

    property IsLocked;
    property LockTimeout;
    property Stream: TStream read getStream write setStream;
  end;

implementation

//------------------------------------------------------------------------------------------------------------------------------------------
{ TBaseLocker }

constructor TBaseLocker.Create(AOwner: TComponent);
begin
  inherited;

  FSem := TNamedSemaphore.Create(format('%s%f', [Classname, now]));
end;

destructor TBaseLocker.Destroy;
begin
  while isLocked do
    Unlock;

  FSem.free;

  inherited;

end;

function TBaseLocker.GetIsLocked: boolean;
begin
  Result := FSem.CanLock;
end;

function TBaseLocker.getSem: TSemaphore;
begin
  Result := FSem;
end;

function TBaseLocker.GetSemaphoreHandle: cardinal;
begin
  Result := FSem.SemaphoreHandle;
end;

procedure TBaseLocker.SetLockTimeout(const Value: cardinal);
begin
  FSem.Timeout := Value;
end;

procedure TBaseLocker.Unlock;
begin
  FSem.Unlock;
end;

//------------------------------------------------------------------------------------------------------------------------------------------
{ TBaseComponentLocker }

procedure TBaseComponentLocker.Notification(AComponent: TComponent; Operation: TOperation);
var
  WasLocked: boolean;
begin
  if (Operation = opRemove) and (AComponent = FComponent) then begin
    WasLocked := IsLocked;
    while IsLocked do Unlock;
    FComponent := nil;
    if WasLocked then raise ELockerError.Create('Component was locked when it was removed!');
  end;

  inherited;
end;

procedure TBaseComponentLocker.SetComponent(const Value: TComponent);
begin
  FComponent := Value;
end;

//------------------------------------------------------------------------------------------------------------------------------------------
{ TCustomComponentLocker }

function TCustomComponentLocker.Lock: TComponent;
begin
  Lock(Result);
end;

function TCustomComponentLocker.Lock(var AComponent: TComponent): boolean;
begin
  Result := FSem.Lock;
  if not Result then begin
    AComponent := nil;
  end
  else
    AComponent := FComponent;
end;

//------------------------------------------------------------------------------------------------------------------------------------------
{ TComponentLocker }

function TComponentLocker.Lock: TComponent;
begin
  Result := Inherited Lock;
end;

//------------------------------------------------------------------------------------------------------------------------------------------
{ TBaseObjectLocker }

function TBaseObjectLocker.DoLock(var AObject: TObject; useTimeout: boolean = false; ATimeout: cardinal = 1000 ): boolean;
begin
  if useTimeout then
    Result := FSem.LockEx(ATimeout)
  else
    Result := FSem.Lock;
  if not Result then begin
    AObject := nil;
  end
  else
    AObject := FSubject;
end;

procedure TBaseObjectLocker.Notification(AComponent: TComponent; Operation: TOperation);
var
  WasLocked: boolean;
begin
  //This routine isn't really needed, but as all classes inherit from TObject, for sanity I'm including it..

  if (Operation = opRemove) and (AComponent = FSubject) then begin
    WasLocked := IsLocked;
    while IsLocked do Unlock;
    FSubject := nil;
    if WasLocked then raise ELockerError.Create('Component was locked when it was removed!');
  end;
  inherited;
end;

procedure TBaseObjectLocker.SetSubject(const Value: TObject);
begin
  FSubject := Value;
end;

//------------------------------------------------------------------------------------------------------------------------------------------
{ TCustomListLocker }

function TCustomListLocker.GetList: TList;
begin
  Result := FSubject as TList;
end;

function TCustomListLocker.Lock: TList;
var
  Temp: TObject;
begin
  if DoLock(Temp) then
    Result := Temp as TList
  else
    Result := nil;
end;

procedure TCustomListLocker.SetList(const Value: TList);
begin
  FSubject := Value;
end;

//------------------------------------------------------------------------------------------------------------------------------------------
{ TListLocker }

function TListLocker.Lock: TList;
begin
  Result := inherited Lock;
end;

//------------------------------------------------------------------------------------------------------------------------------------------
function TListLocker.LockEx(const ATimeout: cardinal): TList;
var
  Temp: TObject;
begin
  if DoLock(Temp, true, ATimeout) then
    Result := Temp as TList
  else
    Result := nil;
end;

{ TObjectListLocker }

function TObjectListLocker.GetObjectList: TObjectList;
begin
  Result := GetList as TObjectList;
end;

function TObjectListLocker.Lock: TObjectList;
begin
  result := inherited Lock as TObjectList;
end;

function TObjectListLocker.LockEx(ATimeout: integer): TObjectList;
var
  Temp: TObject;
begin
  if DoLock(Temp, true, ATimeout) then
    Result := Temp as TObjectList
  else
    Result := nil;
end;

procedure TObjectListLocker.SetObjectList(const Value: TObjectList);
begin
  SetList(Value);
end;

//------------------------------------------------------------------------------------------------------------------------------------------
{ TStringsLocker }

function TStringsLocker.GetStrings: TStrings;
begin
  Result := FSubject as TStrings;
end;

function TStringsLocker.Lock: TStrings;
var
  Temp: TObject;
begin
  if DoLock(Temp) then
    Result := Temp as TStrings
  else
    Result := nil;

end;

procedure TStringsLocker.SetStrings(const Value: TStrings);
begin
  FSubject := Value;
end;

//------------------------------------------------------------------------------------------------------------------------------------------
{ TFormLocker }

function TFormLocker.GetForm: TForm;
begin
  Result := FComponent as TForm;
end;

function TFormLocker.Lock: TForm;
begin
  Result := inherited Lock as TForm;
end;

procedure TFormLocker.SetForm(const Value: TForm);
begin
  FComponent := Value;
end;

//------------------------------------------------------------------------------------------------------------------------------------------
{ TFrameLocker }

function TFrameLocker.GetFrame: TFrame;
begin
  Result := FComponent as TFrame;
end;

function TFrameLocker.Lock: TFrame;
begin
  Result := inherited Lock as TFrame;
end;

procedure TFrameLocker.SetFrame(const Value: TFrame);
begin
  FComponent := Value;
end;

//------------------------------------------------------------------------------------------------------------------------------------------

{ TStreamLocker }

function TStreamLocker.getStream: TStream;
begin
  result := FSubject as TStream;
end;

function TStreamLocker.Lock: TStream;
var
  Temp: TObject;
begin
  if DoLock(Temp) then
    Result := Temp as TStream
  else
    Result := nil;
end;

procedure TStreamLocker.setStream(const Value: TStream);
begin
  FSubject := Value;
end;

//--------------------------------------------------------------------------------------------------

{ TInterfaceListLocker }

function TInterfaceListLocker.GetInterfaceList: TInterfaceList;
begin
  result := FSubject as TInterfaceList;
end;

function TInterfaceListLocker.Lock: TInterfaceList;
var
  Temp: TObject;
begin
  if DoLock(Temp) then
    Result := Temp as TInterfaceList
  else
    Result := nil;
end;

function TInterfaceListLocker.LockEx(ATimeout: integer): TInterfaceList;
var
  Temp: TObject;
begin
  if DoLock(Temp, true, ATimeout) then
    Result := Temp as TInterfaceList
  else
    Result := nil;
end;


procedure TInterfaceListLocker.SetInterfaceList(const Value: TInterfaceList);
begin
  FSubject := value;
end;

{ TOrderedListLocker }

function TOrderedListLocker.GetOrderedList: TOrderedList;
begin
  Result := FSubject as TOrderedList;
end;

function TOrderedListLocker.Lock: TOrderedList;
var
  Temp: TObject;
begin
  if DoLock(Temp) then
    Result := Temp as TOrderedList
  else
    Result := nil;
end;

procedure TOrderedListLocker.SetOrderedList(const Value: TOrderedList);
begin
  FSubject := Value;
end;

end.
