{-----------------------------------------------------------------------------
 Unit Name: Semaphore
 Author:    mathew emson
 Date:      21-Oct-2001
 Purpose:
 History:   Matt Emson 20021021

            0.0.1 - basic functionality there, but some areas need a little work.
            0.0.2 - Added two further classes 'TNamedSemaphore' and 'TNamedBlockingSemaphore'.
                    These accomodate the need to acquire semaphores already active in other
                    processes. So long as you use the exact same name in the same case, the
                    semaphore will be shared. NB. Acquiring a semaphore will allow the
                    semaphore to be closed by a second semaphore class. This should not close
                    the handle for all other classes who own the handle, but be carefull
                    none the less..
            0.0.3 - Now tried and tested old workhorse !!

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




unit Semaphore;

interface

uses
  Classes, windows, messages, sysutils;

const
  //Why did Borland not include this in 'Windows'???
  //It's in Borland's BCC32 v5.5 'Winnt.h' header file...
  SEMAPHORE_ALL_ACCESS = STANDARD_RIGHTS_REQUIRED or SYNCHRONIZE or $3;

type
  // ----------------------------------------------------------------------------------------------
  //                                TBaseSemaphore                    TSingleInstance
  //                  TBlockingSemaphore      TSemaphore
  //              TNamedBlockingSemaphore     TNamedSemaphore
  // ----------------------------------------------------------------------------------------------
  TBaseSemaphore = class(TObject)
  private
    FSemaphore: Thandle;
    FlockCount: Integer;
    FSemCount: integer;

    function GetUnreliableLockCount: integer;

  protected
    FName: PChar;
    //you must add a lock method to enable locking to take place...
    //e.g.
    //  procedure Lock;
    //  function Lock: boolean;

    //unlocks one level of lock. If you use the default lock level of 1, this
    //works well, but otherwise remember to unlock the same number of times you lock.
    procedure Unlock; virtual;
    //most flakey at the best of times. needs to be improved - interlockedexchange will
    //probably yield better results.
    function CanLock: boolean; virtual;
    //defaults the lock count to 1

  public
    constructor Create(count: cardinal = 1); virtual;
    destructor Destroy; override;

    property UnreliableLockCount: integer read GetUnreliableLockCount;
    property SemaphoreHandle: THandle read FSemaphore;
  end;

  // ----------------------------------------------------------------------------------------------
  TBlockingSemaphore = class(TBaseSemaphore)
  public
    //CAUTION! Calling lock will block until enough semaphores
    //         are unlocked to allow another semaphore to be created.
    //         Always take a look at 'CanLock' if in a single threaded
    //         environment.
    procedure Lock; virtual;
    procedure Unlock; override;                                                                //brought forward
    function CanLock: boolean; override;                                                       //brought forward
  end;

  // ----------------------------------------------------------------------------------------------
  TNamedBlockingSemaphore = class(TBlockingSemaphore)
  public
    function Acquire(const AName: string): boolean;

    {will acquire sem if is is already in existance..}
    constructor Create(AName: string; count: cardinal = 1); reintroduce; virtual;
    constructor CreateUnconnected(AName: string = ''); virtual;
    destructor Destroy; override;
  end;

  // ----------------------------------------------------------------------------------------------
  TSemaphore = class(TBaseSemaphore)
  private
    FTimeout: Longword;
    procedure SetTimeout(const Value: Longword);

  public
    //The timeout will happen between the lock call and the success/fail of the
    //lock:
    //     If the lock succeeeds, then wait <= Timeout,
    //     else if lock fails, then wait = timeout;
    //With this in mind, a wait of 1000 (I second) is reasonable, but caustion
    //should be taken with longet timeout's - you could seriously impact on the
    //preformance of the app.
    property Timeout: Longword read FTimeout write SetTimeout;

    //lock returns true if the semaphore was able to acquire a lock
    function Lock: boolean; virtual;
    function LockEx(ATimeOut: longword): boolean;
    procedure Unlock; override;                                                                //brought forward
    function CanLock: boolean; override;                                                       //brought forward

    constructor Create(count: cardinal = 1); override;
  end;

  // ----------------------------------------------------------------------------------------------
  TNamedSemaphore = class(TSemaphore)
  public
    function Acquire(const AName: string): boolean;
    constructor Create(AName: string; count: cardinal = 1); reintroduce; virtual;
    constructor CreateUnconnected(AName: string = ''); virtual;
    destructor Destroy; override;
  end;

  // ----------------------------------------------------------------------------------------------
  TSingleInstance = class
  private
    FSemaphore: THandle;
    FToken: string;
    FInstalled: Boolean;
  public
    constructor Create(Token: string); reintroduce; virtual;
    function InstallSingleInstance: boolean;
    procedure UninstallSingleInstance;
    destructor Destroy; override;
  end;

  // ----------------------------------------------------------------------------------------------

implementation

// ------------------------------------------------------------------------------------------------
//  TBaseSemaphore
// ------------------------------------------------------------------------------------------------

constructor TBaseSemaphore.Create(count: cardinal);
begin
  FlockCount := 0;
  FSemCount := count;

  FSemaphore := CreateSemaphore(nil, count, count, FName);

  //dwritefmt('create_sem %d - %s."%s" count %d', [FSemaphore, Classname, FName, FLockCount]);
end;

// ------------------------------------------------------------------------------------------------

destructor TBaseSemaphore.Destroy;
var
  i: integer;
begin
  // why bother?  surely just close the semaphore?????

  if (FlockCount > 0) then
  begin
    //dwritefmt('Lock error %d - %s."%s" count %d', [FSemaphore, Classname, FName, FLockCount]);
    for i := FlockCount - 1 downto 0 do
    begin
      Unlock;
    end;
  end;

  CloseHandle(FSemaphore);

  //dwritefmt('delete_sem %d - %s."%s" count %d', [FSemaphore, Classname, FName, FLockCount]);

  inherited;
end;

// ------------------------------------------------------------------------------------------------

function TBaseSemaphore.CanLock: boolean;
begin
  Result := (FSemCount < FlockCount);                                                          // dodgy ground.. may give false
  // results in extreme threading
end;

// ------------------------------------------------------------------------------------------------

procedure TBaseSemaphore.Unlock;
begin
  ReleaseSemaphore(FSemaphore, 1, nil);

  if (FLockCount > 0) then
    InterlockedDecrement(FLockCount);
  //else
      {if (FName <> nil) then begin
        dwritefmt('attempt to unlock > count %d - %s."%s"', [FSemaphore, Classname, FName])
      end
      else
        dwritefmt('attempt to unlock > count %d - %s."%s"', [FSemaphore, Classname, '']);

  if (FName <> nil) then
    dwritefmt('Unlock %d - %s."%s" count %d', [FSemaphore, Classname, FName, FLockCount])
  else
    dwritefmt('Unlock %d - %s."%s" count %d', [FSemaphore, Classname, '', FLockCount]); }
end;

// ------------------------------------------------------------------------------------------------

function TBaseSemaphore.GetUnreliableLockCount: integer;
begin
  //dwritefmt('GetUnreliableLockCount(1) %d', [FLockCount]);
  Result := FLockCount;
  //dwritefmt('GetUnreliableLockCount(2) %d', [FLockCount]);
  sleep(0);
  //dwritefmt('GetUnreliableLockCount(3) %d', [FLockCount]);
end;



// ------------------------------------------------------------------------------------------------
//  TSemaphore
// ------------------------------------------------------------------------------------------------

constructor TSemaphore.Create(count: cardinal);
begin
  FTimeout := 1000;                                                                            // 1 second;

  inherited;

  {if (FName <> nil) then
    dwritefmt('create_sem %d - %s."%s" count %d', [FSemaphore, Classname, FName, FLockCount])
  else
    dwritefmt('create_sem %d - %s."%s" count %d', [FSemaphore, Classname, '', FLockCount]); }
end;

// ------------------------------------------------------------------------------------------------

function TSemaphore.Lock: boolean;
begin
  //dwritefmt('Before Lock %d - %s."%s" count %d', [FSemaphore, Classname, FName, FLockCount]);
  {Result := false;
  if WaitForSingleObject(FSemaphore, FTimeout) = WAIT_OBJECT_0 then begin
    Result := True;
    InterlockedIncrement(FLockCount);
  end; }

  //1_4_2_2  -  needed a way to specify the timeout...
  Result := LockEx(FTimeout);

  //dwritefmt('Lock %d - %s."%s" count %d', [FSemaphore, Classname, FName, FLockCount]);
end;


// ------------------------------------------------------------------------------------------------

function TSemaphore.LockEx (ATimeOut: longword): boolean;
begin
  Result := (WaitForSingleObject(FSemaphore, ATimeout) = WAIT_OBJECT_0);

  if Result then
    InterlockedIncrement(FLockCount);
end;

// ------------------------------------------------------------------------------------------------

procedure TSemaphore.Unlock;
begin
  inherited;
end;

// ------------------------------------------------------------------------------------------------

function TSemaphore.CanLock: boolean;
begin
  result := inherited CanLock;

  //dwritefmt('can lock %d - %s."%s" count %d', [FSemaphore, Classname, FName, FLockCount]);
end;

// ------------------------------------------------------------------------------------------------

procedure TSemaphore.SetTimeout(const Value: Longword);
begin
  FTimeout := Value;
end;


// ------------------------------------------------------------------------------------------------
//  BlockingSemaphore
// ------------------------------------------------------------------------------------------------

procedure TBlockingSemaphore.Lock;
begin
  {if (FName <> nil) then
    dwritefmt('Before Lock %d - %s."%s" count %d', [FSemaphore, Classname, FName, FLockCount])
  else
    dwritefmt('Before Lock %d - %s."%s" count %d', [FSemaphore, Classname, '', FLockCount]); }

  while WaitForSingleObject(FSemaphore, INFINITE) <> WAIT_OBJECT_0 do
    sleep(0);                                                                                  //release the context to allow other threads to run.

  InterlockedIncrement(FLockCount);

  {if (FName <> nil) then
    dwritefmt('Lock %d - %s."%s" count %d', [FSemaphore, Classname, FName, FLockCount])
  else
    dwritefmt('Lock %d - %s."%s" count %d', [FSemaphore, Classname, '', FLockCount]); }
end;

// ------------------------------------------------------------------------------------------------

procedure TBlockingSemaphore.Unlock;
begin
  inherited;
end;

// ------------------------------------------------------------------------------------------------

function TBlockingSemaphore.CanLock: boolean;
begin
  result := inherited CanLock;
end;

// ------------------------------------------------------------------------------------------------
//  TNamedBlockingSemaphore
// ------------------------------------------------------------------------------------------------


function TNamedBlockingSemaphore.Acquire(const AName: string): boolean;
begin
  Result := True;

  if FSemaphore > 0 then
    CloseHandle(FSemaphore);

  if FName <> nil then StrDispose(FName);
  FName := StrAlloc(length(aname) + 1);
  StrPCopy(FName, pchar(AName));

  FSemaphore := OpenSemaphore(SEMAPHORE_ALL_ACCESS, True, FName);

  if FSemaphore = 0 then
    Result := False;
end;

constructor TNamedBlockingSemaphore.Create(AName: string; count: cardinal);
begin
  FlockCount := 0;
  FSemCount := count;

  if FName <> nil then StrDispose(FName);
  FName := StrAlloc(length(aname) + 1);
  StrPCopy(FName, pchar(AName));

  FSemaphore := OpenSemaphore(SEMAPHORE_ALL_ACCESS, True, FName);

  if FSemaphore = 0 then
    inherited Create(count)
      { else
       dwritefmt('create_sem %d - %s."%s" count %d', [FSemaphore, Classname, FName, FLockCount])};
end;

constructor TNamedBlockingSemaphore.CreateUnconnected(AName: string);
begin
  FlockCount := 0;
  FSemCount := -1;                                                                             //can't be set if not connected because the only way
  //to activate the class is to call 'Acquire' and this
  //will fail if the semaphore doesn't exist already, quid-pro-quo
  //the count will already be set up

  if FName <> nil then StrDispose(FName);
  FName := StrAlloc(length(aname) + 1);
  StrPCopy(FName, pchar(AName));

  FSemaphore := 0;

  //dwritefmt('create_sem_ex %d - %s."%s" count %d', [FSemaphore, Classname, FName, FLockCount]);
end;

destructor TNamedBlockingSemaphore.Destroy;
begin
  if FName <> nil then StrDispose(FName);

  inherited;
end;

{ TNamedSemaphore }

function TNamedSemaphore.Acquire(const AName: string): boolean;
begin
  Result := True;

  if FSemaphore > 0 then
    CloseHandle(FSemaphore);

  if FName <> nil then StrDispose(FName);
  FName := StrAlloc(length(aname) + 1);
  StrPCopy(FName, pchar(AName));

  FSemaphore := OpenSemaphore(SEMAPHORE_ALL_ACCESS, True, FName);

  if FSemaphore = 0 then
    Result := False;
end;

constructor TNamedSemaphore.Create(AName: string; count: cardinal);
begin
  FlockCount := 0;
  FSemCount := count;

  if FName <> nil then StrDispose(FName);
  FName := StrAlloc(length(aname) + 1);
  StrPCopy(FName, pchar(AName));

  FSemaphore := OpenSemaphore(SEMAPHORE_ALL_ACCESS, True, FName);

  if FSemaphore = 0 then
    inherited Create(count);
  //else
    //dwritefmt('create_sem %d - %s."%s" count %d', [FSemaphore, Classname, FName, FLockCount]);

  FTimeout := 1000;
end;

constructor TNamedSemaphore.CreateUnconnected(AName: string);
begin
  FlockCount := 0;
  FSemCount := -1;                                                                             //can't be set if not connected because the only way
  //to activate the class is to call 'Acquire' and this
  //will fail if the semaphore doesn't exist already, quid-pro-quo
  //the count will already be set up

  if FName <> nil then StrDispose(FName);
  FName := StrAlloc(length(aname) + 1);
  StrPCopy(FName, pchar(AName));

  FTimeout := 1000;

  //dwritefmt('create_sem_ex %d - %s."%s" count %d', [FSemaphore, Classname, FName, FLockCount]);
end;

destructor TNamedSemaphore.Destroy;
begin
  if FName <> nil then StrDispose(FName);

  inherited;
end;

{ TSingleInstance }

constructor TSingleInstance.Create(Token: string);
begin
  FToken := Token;
  FInstalled := false;

  //dwritefmt('singleinstance_sem "%s"', [FToken]);
end;

destructor TSingleInstance.Destroy;
begin
  if FInstalled then
    UninstallSingleInstance;

  inherited;
end;

function TSingleInstance.InstallSingleInstance: boolean;
begin
  FSemaphore := OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, pchar(FToken));

  if (0 <> FSemaphore) then
    begin
      // already exists...
      Result := False;
      UninstallSingleInstance;
    end
  else
    // (ERROR_FILE_NOT_FOUND = GetLastError)
    begin
      FSemaphore := CreateSemaphore (nil, 1, 1, pchar(FToken));

    // possible race between opensemaphore and createsemaphore......

    if (GetLastError = ERROR_ALREADY_EXISTS) then
      FSemaphore := OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, pchar(FToken));

    if (FSemaphore = 0) or (GetLastError = ERROR_ALREADY_EXISTS) then
      Result := False
    else
      Result := True;

    FInstalled := Result;
  end;
end;

procedure TSingleInstance.UninstallSingleInstance;
begin
  FInstalled := false;

  ReleaseSemaphore(FSemaphore, 1, nil);
  closehandle(FSemaphore);
end;


end.





