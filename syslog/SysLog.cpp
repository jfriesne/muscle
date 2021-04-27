/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>
#include <stdarg.h>
#include "dataio/FileDataIO.h"
#include "regex/StringMatcher.h"
#include "syslog/LogCallback.h"
#include "system/SetupSystem.h"
#include "system/SystemInfo.h"  // for GetFilePathSeparator()
#include "util/Directory.h"
#include "util/FilePathInfo.h"
#include "util/Hashtable.h"
#include "util/NestCount.h"
#include "util/String.h"
#include "util/StringTokenizer.h"

#ifdef MUSCLE_USE_MSVC_STACKWALKER
# include <dbghelp.h>
# if defined(UNICODE) && !defined(_UNICODE)
#  define _UNICODE 1
# endif
# include <tchar.h>
#endif

#if !defined(MUSCLE_INLINE_LOGGING) && defined(MUSCLE_ENABLE_ZLIB_ENCODING)
# include "zlib/zlib/zlib.h"
#endif

#if defined(__APPLE__)
# include "AvailabilityMacros.h"  // so we can find out if this version of MacOS/X is new enough to include backtrace() and friends
#endif

#if (defined(__linux__) && !defined(ANDROID)) || (defined(MAC_OS_X_VERSION_10_5) && defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5))
# include <execinfo.h>
# define MUSCLE_USE_BACKTRACE 1
#endif

// Work-around for Android not providing timegm()... we'll just include the implementation inline, right here!  --jaf, jfm
#if defined(ANDROID)
/*
 * Copyright (c) 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
# include <sys/types.h>
# include <time.h>
static bool is_leap(unsigned y) {y += 1900; return (y % 4) == 0 && ((y % 100) != 0 || (y % 400) == 0);}
static time_t timegm (struct tm *tm)
{
   static const unsigned ndays[2][12] ={{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}, {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};
   time_t res = 0;
   for (int i = 70; i < tm->tm_year; ++i) res += is_leap(i) ? 366 : 365;
   for (int i = 0;  i < tm->tm_mon;  ++i) res += ndays[is_leap(tm->tm_year)][i];
   res += tm->tm_mday - 1; res *= 24;
   res += tm->tm_hour;     res *= 60;
   res += tm->tm_min;      res *= 60;
   res += tm->tm_sec;
   return res;
}
#endif

namespace muscle {

#ifdef THIS_FUNCTION_IS_NOT_ACTUALLY_USED_I_JUST_KEEP_IT_HERE_SO_I_CAN_QUICKLY_COPY_AND_PASTE_IT_INTO_THIRD_PARTY_CODE_WHEN_NECESSARY_SAYS_JAF
# include <execinfo.h>
void PrintStackTrace()
{
   FILE * optFile = stdout;
   void *array[256];
   size_t size = backtrace(array, 256);
   char ** strings = backtrace_symbols(array, 256);
   if (strings)
   {
      fprintf(optFile, "--Stack trace follows (%i frames):\n", (int) size);
      for (size_t i = 0; i < size; i++) fprintf(optFile, "  %s\n", strings[i]);
      fprintf(optFile, "--End Stack trace\n");
      free(strings);
   }
   else fprintf(optFile, "PrintStackTrace:  Error, could not generate stack trace!\n");
}
#endif

// Begin windows stack trace code.  This code is simplified, so it Only works for _MSC_VER >= 1300
#if defined(MUSCLE_USE_MSVC_STACKWALKER) && !defined(MUSCLE_INLINE_LOGGING)

/**********************************************************************
 *
 * Liberated from:
 *
 *   http://www.codeproject.com/KB/threads/StackWalker.aspx
 *
 **********************************************************************/

class StackWalkerInternal;  // forward
class StackWalker
{
public:
   typedef enum StackWalkOptions
   {
     // No additional info will be retrieved
     // (only the address is available)
     RetrieveNone = 0,

     // Try to get the symbol-name
     RetrieveSymbol = 1,

     // Try to get the line for this symbol
     RetrieveLine = 2,

     // Try to retrieve the module-infos
     RetrieveModuleInfo = 4,

     // Also retrieve the version for the DLL/EXE
     RetrieveFileVersion = 8,

     // Contains all the abouve
     RetrieveVerbose = 0xF,

     // Generate a "good" symbol-search-path
     SymBuildPath = 0x10,

     // Also use the public Microsoft-Symbol-Server
     SymUseSymSrv = 0x20,

     // Contains all the above "Sym"-options
     SymAll = 0x30,

     // Contains all options (default)
     OptionsAll = 0x3F,

     OptionsJAF = (RetrieveSymbol|RetrieveLine)
   } StackWalkOptions;

   StackWalker(
     FILE * optOutFile,
     String * optOutString,
     int options = OptionsAll, // 'int' is by design, to combine the enum-flags
     LPTSTR szSymPath = NULL,
     DWORD dwProcessId = GetCurrentProcessId(),
     HANDLE hProcess = GetCurrentProcess()
     );
   ~StackWalker();

   typedef BOOL (__stdcall *PReadProcessMemoryRoutine)(
     HANDLE      hProcess,
     DWORD64     qwBaseAddress,
     PVOID       lpBuffer,
     DWORD       nSize,
     LPDWORD     lpNumberOfBytesRead,
     LPVOID      pUserData  // optional data, which was passed in "ShowCallstack"
     );

   BOOL LoadModules();

   BOOL ShowCallstack(
     uint32 maxDepth,
     HANDLE hThread = GetCurrentThread(),
     const CONTEXT *context = NULL,
     PReadProcessMemoryRoutine readMemoryFunction = NULL,
     LPVOID pUserData = NULL  // optional to identify some data in the 'readMemoryFunction'-callback
     );

protected:
    enum {STACKWALK_MAX_NAMELEN = 1024}; // max name length for found symbols

protected:
   // Entry for each Callstack-Entry
   typedef struct CallstackEntry
   {
     DWORD64 offset;  // if 0, we have no valid entry
     TCHAR name[STACKWALK_MAX_NAMELEN];
     TCHAR undName[STACKWALK_MAX_NAMELEN];
     TCHAR undFullName[STACKWALK_MAX_NAMELEN];
     DWORD64 offsetFromSymbol;
     DWORD offsetFromLine;
     DWORD lineNumber;
     TCHAR lineFileName[STACKWALK_MAX_NAMELEN];
     DWORD symType;
     LPCTSTR symTypeString;
     TCHAR moduleName[STACKWALK_MAX_NAMELEN];
     DWORD64 baseOfImage;
     TCHAR loadedImageName[STACKWALK_MAX_NAMELEN];
   } CallstackEntry;

   enum CallstackEntryType {firstEntry, nextEntry, lastEntry};

   void OnSymInit(LPTSTR szSearchPath, DWORD symOptions, LPTSTR szUserName);
   void OnLoadModule(LPTSTR img, LPTSTR mod, DWORD64 baseAddr, DWORD size, DWORD result, LPCTSTR symType, LPTSTR pdbName, ULONGLONG fileVersion);
   void OnCallstackEntry(CallstackEntryType eType, CallstackEntry *entry);
   void OnDbgHelpErr(LPCTSTR szFuncName, DWORD gle, DWORD64 addr);
   void OnOutput(LPTSTR szText)
   {
      if (this->m_outFile) _fputts(_T("  "), m_outFile);
                      else _putts(_T("  "));
      if (this->m_outFile) _fputts(szText, m_outFile);
                      else _putts(szText);
      if (this->m_outString)
#ifdef _UNICODE
      {
         size_t len;
         (void)wcstombs_s(&len, NULL, 0, szText, wcslen(szText));
         char *buf = newnothrow char[len];
         (void) wcstombs_s(&len, buf, len, szText, wcslen(szText));
         if (len > 0)
           (*this->m_outString) += buf;
         delete [] buf;
      }
#else
         (*this->m_outString) += szText;
#endif
   }


   StackWalkerInternal *m_sw;
   HANDLE m_hProcess;
   DWORD m_dwProcessId;
   BOOL m_modulesLoaded;
   LPTSTR m_szSymPath;

   FILE * m_outFile;      // added by jaf (because subclassing is overkill for my needs here)
   String * m_outString;  // ditto

   int m_options;

   static BOOL __stdcall ReadProcMemCallback(HANDLE hProcess, DWORD64 qwBaseAddress, PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead);

   friend class StackWalkerInternal;
};

void _Win32PrintStackTraceForContext(FILE * optFile, CONTEXT * context, uint32 maxDepth)
{
   fprintf(optFile, "--Stack trace follows:\n");
   StackWalker(optFile, NULL, StackWalker::OptionsJAF).ShowCallstack(maxDepth, GetCurrentThread(), context);
   fprintf(optFile, "--End Stack trace\n");
}


// Some missing defines (for VC5/6):
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

// Normally it should be enough to use 'CONTEXT_FULL' (better would be 'CONTEXT_ALL')
#define USED_CONTEXT_FLAGS CONTEXT_FULL

class StackWalkerInternal
{
public:
  StackWalkerInternal(StackWalker *parent, HANDLE hProcess)
  {
    m_parent = parent;
    m_hDbhHelp = NULL;
    pSC = NULL;
    m_hProcess = hProcess;
    m_szSymPath = NULL;
    pSFTA = NULL;
    pSGLFA = NULL;
    pSGMB = NULL;
    pSGMI = NULL;
    pSGO = NULL;
#ifdef _UNICODE
    pSFA = NULL;
#else
    pSGSFA = NULL;
#endif
    pSI = NULL;
    pSLM = NULL;
    pSSO = NULL;
    pSW = NULL;
    pUDSN = NULL;
    pSGSP = NULL;
  }
  ~StackWalkerInternal()
  {
    if (pSC != NULL)
      pSC(m_hProcess);  // SymCleanup
    if (m_hDbhHelp != NULL)
      FreeLibrary(m_hDbhHelp);
    m_hDbhHelp = NULL;
    m_parent = NULL;
    if(m_szSymPath != NULL)
      free(m_szSymPath);
    m_szSymPath = NULL;
  }
  BOOL Init(LPTSTR szSymPath)
  {
    if (m_parent == NULL)
      return FALSE;

    // Dynamically load the Entry-Points for dbghelp.dll:
    // First try to load the newest one from
    {
      TCHAR szTemp[4096];
      // But before we do this, we first check if the ".local" file exists
      if (GetModuleFileName(NULL, szTemp, 4096) > 0)
      {
        _tcscat_s(szTemp, _T(".local"));
        if (GetFileAttributes(szTemp) == INVALID_FILE_ATTRIBUTES)
        {
          // ".local" file does not exist, so we can try to load the dbghelp.dll from the "Debugging Tools for Windows"
          LPCTSTR suffixes[] = {
            _T("\\Debugging Tools for Windows\\dbghelp.dll"),
            _T("\\Debugging Tools for Windows 64-Bit\\dbghelp.dll"),
            _T("\\Debugging Tools for Windows (x64)\\dbghelp.dll")
          };
          for (uint32 i = 0; ((i < ARRAYITEMS(suffixes)) && (m_hDbhHelp == NULL)); i++)
          {
            if (GetEnvironmentVariable(_T("ProgramFiles"), szTemp, 4096) > 0)
            {
              _tcscat_s(szTemp, suffixes[i]);
              if (GetFileAttributes(szTemp) != INVALID_FILE_ATTRIBUTES) m_hDbhHelp = LoadLibrary(szTemp);
            }
          }
        }
      }
    }
    if (m_hDbhHelp == NULL)  // if not already loaded, try to load a default-one
      m_hDbhHelp = LoadLibrary(_T("dbghelp.dll"));
    if (m_hDbhHelp == NULL)
      return FALSE;
#ifdef _UNICODE
    pSI = (tSI) GetProcAddress(m_hDbhHelp, "SymInitializeW");
#else
    pSI = (tSI) GetProcAddress(m_hDbhHelp, "SymInitialize");
#endif
    pSC = (tSC) GetProcAddress(m_hDbhHelp, "SymCleanup");

    pSW = (tSW) GetProcAddress(m_hDbhHelp, "StackWalk64");
    pSGO = (tSGO) GetProcAddress(m_hDbhHelp, "SymGetOptions");
    pSSO = (tSSO) GetProcAddress(m_hDbhHelp, "SymSetOptions");

    pSFTA = (tSFTA) GetProcAddress(m_hDbhHelp, "SymFunctionTableAccess64");
#ifdef _UNICODE
    pSGLFA = (tSGLFA) GetProcAddress(m_hDbhHelp, "SymGetLineFromAddrW64");
#else
    pSGLFA = (tSGLFA) GetProcAddress(m_hDbhHelp, "SymGetLineFromAddr64");
#endif
    pSGMB = (tSGMB) GetProcAddress(m_hDbhHelp, "SymGetModuleBase64");
#ifdef _UNICODE
    pSGMI = (tSGMI) GetProcAddress(m_hDbhHelp, "SymGetModuleInfoW64");
    pSFA = (tSFA) GetProcAddress(m_hDbhHelp, "SymFromAddrW");
    pUDSN = (tUDSN) GetProcAddress(m_hDbhHelp, "UnDecorateSymbolNameW");
    pSLM = (tSLM)GetProcAddress(m_hDbhHelp, "SymLoadModuleExW");
    pSGSP = (tSGSP)GetProcAddress(m_hDbhHelp, "SymGetSearchPathW");
#else
    pSGMI = (tSGMI) GetProcAddress(m_hDbhHelp, "SymGetModuleInfo64");
    //pSGMI_V3 = (tSGMI_V3) GetProcAddress(m_hDbhHelp, "SymGetModuleInfo64" );
    pSGSFA = (tSGSFA) GetProcAddress(m_hDbhHelp, "SymGetSymFromAddr64");
    pUDSN = (tUDSN) GetProcAddress(m_hDbhHelp, "UnDecorateSymbolName");
    pSLM = (tSLM) GetProcAddress(m_hDbhHelp, "SymLoadModule64");
    pSGSP = (tSGSP) GetProcAddress(m_hDbhHelp, "SymGetSearchPath");
#endif

    if ( pSC == NULL || pSFTA == NULL || pSGMB == NULL || pSGMI == NULL ||
      pSGO == NULL || pSI == NULL || pSSO == NULL ||
#ifdef _UNICODE
      pSFA == NULL ||
#else
      pSGSFA == NULL ||
#endif
      pSW == NULL || pUDSN == NULL || pSLM == NULL )
    {
      FreeLibrary(m_hDbhHelp);
      m_hDbhHelp = NULL;
      pSC = NULL;
      return FALSE;
    }

    // SymInitialize
    if (szSymPath != NULL)
      m_szSymPath = _tcsdup(szSymPath);
    if (this->pSI(m_hProcess, m_szSymPath, FALSE) == FALSE)
      this->m_parent->OnDbgHelpErr(_T("SymInitialize"), GetLastError(), 0);

    DWORD symOptions = this->pSGO();  // SymGetOptions
    symOptions |= SYMOPT_LOAD_LINES;
    symOptions |= SYMOPT_FAIL_CRITICAL_ERRORS;
    //symOptions |= SYMOPT_NO_PROMPTS;
    // SymSetOptions
    symOptions = this->pSSO(symOptions);

    TCHAR buf[StackWalker::STACKWALK_MAX_NAMELEN] = {0};
    if (this->pSGSP != NULL)
    {
      if (this->pSGSP(m_hProcess, buf, StackWalker::STACKWALK_MAX_NAMELEN) == FALSE)
        this->m_parent->OnDbgHelpErr(_T("SymGetSearchPath"), GetLastError(), 0);
    }
    TCHAR szUserName[1024] = {0};
    DWORD dwSize = 1024;
    GetUserName(szUserName, &dwSize);
    this->m_parent->OnSymInit(buf, symOptions, szUserName);

    return TRUE;
  }

  StackWalker *m_parent;

  HMODULE m_hDbhHelp;
  HANDLE m_hProcess;
  LPTSTR m_szSymPath;

  // SymCleanup()
  typedef BOOL (__stdcall *tSC)( IN HANDLE hProcess );
  tSC pSC;

  // SymFunctionTableAccess64()
  typedef PVOID (__stdcall *tSFTA)( HANDLE hProcess, DWORD64 AddrBase );
  tSFTA pSFTA;

  // SymGetLineFromAddr64()
#ifdef _UNICODE
  typedef BOOL (__stdcall *tSGLFA)( IN HANDLE hProcess, IN DWORD64 dwAddr, OUT PDWORD pdwDisplacement, OUT PIMAGEHLP_LINEW64 Line );
#else
  typedef BOOL (__stdcall *tSGLFA)( IN HANDLE hProcess, IN DWORD64 dwAddr, OUT PDWORD pdwDisplacement, OUT PIMAGEHLP_LINE64 Line );
#endif
  tSGLFA pSGLFA;

  // SymGetModuleBase64()
  typedef DWORD64 (__stdcall *tSGMB)( IN HANDLE hProcess, IN DWORD64 dwAddr );
  tSGMB pSGMB;

  // SymGetModuleInfo64()
#ifdef _UNICODE
  typedef BOOL (__stdcall *tSGMI)( IN HANDLE hProcess, IN DWORD64 dwAddr, OUT IMAGEHLP_MODULEW64 *ModuleInfo);
#else
  typedef BOOL (__stdcall *tSGMI)( IN HANDLE hProcess, IN DWORD64 dwAddr, OUT IMAGEHLP_MODULE64 *ModuleInfo);
#endif
  tSGMI pSGMI;

  // SymGetOptions()
  typedef DWORD (__stdcall *tSGO)( VOID );
  tSGO pSGO;

#ifdef _UNICODE
  // SymFromAddrW()
  typedef BOOL (__stdcall *tSFA)(IN HANDLE hProcess, IN DWORD64 Address, OUT PDWORD64 Displacement, OUT PSYMBOL_INFOW Symbol );
  tSFA pSFA;
#else
  // SymGetSymFromAddr64()
  typedef BOOL (__stdcall *tSGSFA)( IN HANDLE hProcess, IN DWORD64 dwAddr, OUT PDWORD64 pdwDisplacement, OUT PIMAGEHLP_SYMBOL64 Symbol );
  tSGSFA pSGSFA;
#endif

  // SymInitialize()
  typedef BOOL (__stdcall *tSI)( IN HANDLE hProcess, IN PTSTR UserSearchPath, IN BOOL fInvadeProcess );
  tSI pSI;

#ifdef _UNICODE
  // SymLoadModuleEx()
  typedef DWORD64(__stdcall *tSLM)( IN HANDLE hProcess, IN HANDLE hFile, IN PTSTR ImageName, IN PTSTR ModuleName, IN DWORD64 BaseOfDll, IN DWORD SizeOfDll, IN PMODLOAD_DATA Data, IN DWORD Flags);
#else
  // SymLoadModule64()
  typedef DWORD64 (__stdcall *tSLM)( IN HANDLE hProcess, IN HANDLE hFile, IN PTSTR ImageName, IN PTSTR ModuleName, IN DWORD64 BaseOfDll, IN DWORD SizeOfDll );
#endif
  tSLM pSLM;

  // SymSetOptions()
  typedef DWORD (__stdcall *tSSO)( IN DWORD SymOptions );
  tSSO pSSO;

  // StackWalk64()
  typedef BOOL (__stdcall *tSW)(
    DWORD MachineType,
    HANDLE hProcess,
    HANDLE hThread,
    LPSTACKFRAME64 StackFrame,
    PVOID ContextRecord,
    PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
    PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
    PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
    PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress );
  tSW pSW;

  // UnDecorateSymbolName()
  typedef DWORD (__stdcall WINAPI *tUDSN)( PTSTR DecoratedName, PTSTR UnDecoratedName, DWORD UndecoratedLength, DWORD Flags );
  tUDSN pUDSN;

  typedef BOOL (__stdcall WINAPI *tSGSP)(HANDLE hProcess, PTSTR SearchPath, DWORD SearchPathLength);
  tSGSP pSGSP;

private:
  // **************************************** ToolHelp32 ************************
  #define MAX_MODULE_NAME32 255
  #define TH32CS_SNAPMODULE   0x00000008
  #pragma pack( push, 8 )
  typedef struct tagMODULEENTRY32
  {
      DWORD   dwSize;
      DWORD   th32ModuleID;       // This module
      DWORD   th32ProcessID;      // owning process
      DWORD   GlblcntUsage;       // Global usage count on the module
      DWORD   ProccntUsage;       // Module usage count in th32ProcessID's context
      BYTE  * modBaseAddr;        // Base address of module in th32ProcessID's context
      DWORD   modBaseSize;        // Size in bytes of module starting at modBaseAddr
      HMODULE hModule;            // The hModule of this module in th32ProcessID's context
      TCHAR   szModule[MAX_MODULE_NAME32 + 1];
      TCHAR   szExePath[MAX_PATH];
  } MODULEENTRY32;
  typedef MODULEENTRY32 *  PMODULEENTRY32;
  typedef MODULEENTRY32 *  LPMODULEENTRY32;
  #pragma pack( pop )

  BOOL GetModuleListTH32(HANDLE hProcess, DWORD pid)
  {
    // CreateToolhelp32Snapshot()
    typedef HANDLE (__stdcall *tCT32S)(DWORD dwFlags, DWORD th32ProcessID);
    // Module32First()
    typedef BOOL (__stdcall *tM32F)(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
    // Module32Next()
    typedef BOOL (__stdcall *tM32N)(HANDLE hSnapshot, LPMODULEENTRY32 lpme);

    // try both dlls...
    const TCHAR *dllname[] = { _T("kernel32.dll"), _T("tlhelp32.dll") };
    HINSTANCE hToolhelp = NULL;
    tCT32S pCT32S = NULL;
    tM32F pM32F = NULL;
    tM32N pM32N = NULL;

    HANDLE hSnap;
    MODULEENTRY32 me;
    me.dwSize = sizeof(me);
    BOOL keepGoing;
    size_t i;

    for (i = 0; i<(sizeof(dllname) / sizeof(dllname[0])); i++ )
    {
      hToolhelp = LoadLibrary( dllname[i] );
      if (hToolhelp == NULL)
        continue;
      pCT32S = (tCT32S) GetProcAddress(hToolhelp, "CreateToolhelp32Snapshot");
#ifdef _UNICODE
      pM32F = (tM32F) GetProcAddress(hToolhelp, "Module32FirstW");
      pM32N = (tM32N) GetProcAddress(hToolhelp, "Module32NextW");
#else
      pM32F = (tM32F) GetProcAddress(hToolhelp, "Module32First");
      pM32N = (tM32N) GetProcAddress(hToolhelp, "Module32Next");
#endif
      if ( (pCT32S != NULL) && (pM32F != NULL) && (pM32N != NULL) )
        break; // found the functions!
      FreeLibrary(hToolhelp);
      hToolhelp = NULL;
    }

    if (hToolhelp == NULL)
      return FALSE;

    hSnap = pCT32S( TH32CS_SNAPMODULE, pid );
    if (hSnap == (HANDLE) -1)
      return FALSE;

    keepGoing = !!pM32F( hSnap, &me );
    int cnt = 0;
    while (keepGoing)
    {
      this->LoadModule(hProcess, me.szExePath, me.szModule, (DWORD64) me.modBaseAddr, me.modBaseSize);
      cnt++;
      keepGoing = !!pM32N( hSnap, &me );
    }
    CloseHandle(hSnap);
    FreeLibrary(hToolhelp);
    return (cnt <= 0) ? FALSE : TRUE;
  }  // GetModuleListTH32

  // **************************************** PSAPI ************************
  typedef struct _MODULEINFO {
      LPVOID lpBaseOfDll;
      DWORD SizeOfImage;
      LPVOID EntryPoint;
  } MODULEINFO, *LPMODULEINFO;

  BOOL GetModuleListPSAPI(HANDLE hProcess)
  {
    // EnumProcessModules()
    typedef BOOL (__stdcall *tEPM)(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded );
    // GetModuleFileNameEx()
    typedef DWORD (__stdcall *tGMFNE)(HANDLE hProcess, HMODULE hModule, LPTSTR lpFilename, DWORD nSize );
    // GetModuleBaseName()
    typedef DWORD (__stdcall *tGMBN)(HANDLE hProcess, HMODULE hModule, LPTSTR lpFilename, DWORD nSize );
    // GetModuleInformation()
    typedef BOOL (__stdcall *tGMI)(HANDLE hProcess, HMODULE hModule, LPMODULEINFO pmi, DWORD nSize );

    HINSTANCE hPsapi;
    tEPM pEPM;
    tGMFNE pGMFNE;
    tGMBN pGMBN;
    tGMI pGMI;

    DWORD i;
    //ModuleEntry e;
    DWORD cbNeeded;
    MODULEINFO mi;
    HMODULE *hMods = 0;
    TCHAR *tt = NULL;
    TCHAR *tt2 = NULL;
    const SIZE_T TTBUFLEN = 8096;
    int cnt = 0;

    hPsapi = LoadLibrary(_T("psapi.dll"));
    if (hPsapi == NULL)
      return FALSE;

    pEPM = (tEPM) GetProcAddress( hPsapi, "EnumProcessModules" );
#ifdef _UNICODE
    pGMFNE = (tGMFNE) GetProcAddress( hPsapi, "GetModuleFileNameExW" );
    pGMBN = (tGMFNE) GetProcAddress( hPsapi, "GetModuleBaseNameW" );
#else
    pGMFNE = (tGMFNE) GetProcAddress( hPsapi, "GetModuleFileNameExA" );
    pGMBN = (tGMFNE) GetProcAddress( hPsapi, "GetModuleBaseNameA" );
#endif
    pGMI = (tGMI) GetProcAddress( hPsapi, "GetModuleInformation" );
    if ( (pEPM == NULL) || (pGMFNE == NULL) || (pGMBN == NULL) || (pGMI == NULL) )
    {
      // we couldn´t find all functions
      FreeLibrary(hPsapi);
      return FALSE;
    }

    hMods = (HMODULE*) malloc(sizeof(HMODULE) * (TTBUFLEN / sizeof HMODULE));
    tt = (TCHAR*) malloc(sizeof(TCHAR) * TTBUFLEN);
    tt2 = (TCHAR*) malloc(sizeof(TCHAR) * TTBUFLEN);
    if ( (hMods == NULL) || (tt == NULL) || (tt2 == NULL) )
      goto cleanup;

    if ( ! pEPM( hProcess, hMods, TTBUFLEN, &cbNeeded ) )
    {
      //_fprintf(fLogFile, "%lu: EPM failed, GetLastError = %lu\n", g_dwShowCount, gle );
      goto cleanup;
    }

    if ( cbNeeded > TTBUFLEN )
    {
      //_fprintf(fLogFile, "%lu: More than %lu module handles. Huh?\n", g_dwShowCount, lenof( hMods ) );
      goto cleanup;
    }

    for ( i = 0; i < cbNeeded / sizeof hMods[0]; i++ )
    {
      // base address, size
      pGMI(hProcess, hMods[i], &mi, sizeof mi );
      // image file name
      tt[0] = 0;
      pGMFNE(hProcess, hMods[i], tt, TTBUFLEN );

      // module name
      tt2[0] = 0;
      pGMBN(hProcess, hMods[i], tt2, TTBUFLEN );

      DWORD dwRes = this->LoadModule(hProcess, tt, tt2, (DWORD64) mi.lpBaseOfDll, mi.SizeOfImage);
      if (dwRes != ERROR_SUCCESS) this->m_parent->OnDbgHelpErr(_T("LoadModule"), dwRes, 0);
      cnt++;
    }

  cleanup:
    if (hPsapi != NULL) FreeLibrary(hPsapi);
    if (tt2 != NULL) free(tt2);
    if (tt != NULL) free(tt);
    if (hMods != NULL) free(hMods);

    return cnt != 0;
  }  // GetModuleListPSAPI

  DWORD LoadModule(HANDLE hProcess, LPTSTR img, LPTSTR mod, DWORD64 baseAddr, DWORD size)
  {
    TCHAR *szImg = _tcsdup(img);
    TCHAR *szMod = _tcsdup(mod);
    DWORD result = ERROR_SUCCESS;
    if ( (szImg == NULL) || (szMod == NULL) )
      result = ERROR_NOT_ENOUGH_MEMORY;
    else
    {
#ifdef _UNICODE
      if (pSLM(hProcess, 0, szImg, szMod, baseAddr, size, NULL, 0) == 0)
#else
      if (pSLM(hProcess, 0, szImg, szMod, baseAddr, size) == 0)
#endif
        result = GetLastError();
    }
    ULONGLONG fileVersion = 0;
    if ( (m_parent != NULL) && (szImg != NULL) )
    {
      // try to retrive the file-version:
      if ( (this->m_parent->m_options & StackWalker::RetrieveFileVersion) != 0)
      {
        VS_FIXEDFILEINFO *fInfo = NULL;
        DWORD dwHandle;
        DWORD dwSize = GetFileVersionInfoSize(szImg, &dwHandle);
        if (dwSize > 0)
        {
          LPVOID vData = malloc(dwSize);
          if (vData != NULL)
          {
            if (GetFileVersionInfo(szImg, dwHandle, dwSize, vData) != 0)
            {
              UINT len;
              TCHAR szSubBlock[] = _T("\\");
              if (VerQueryValue(vData, szSubBlock, (LPVOID*) &fInfo, &len) == 0)
                fInfo = NULL;
              else
              {
                fileVersion = ((ULONGLONG)fInfo->dwFileVersionLS) + ((ULONGLONG)fInfo->dwFileVersionMS << 32);
              }
            }
            free(vData);
          }
        }
      }

      // Retrive some additional-infos about the module
#ifdef _UNICODE
      IMAGEHLP_MODULEW64 Module;
#else
      IMAGEHLP_MODULE64 Module;
#endif
      LPCTSTR szSymType = _T("-unknown-");
      if (this->GetModuleInfo(hProcess, baseAddr, &Module) != FALSE)
      {
        switch(Module.SymType)
        {
          case SymNone:
            szSymType = _T("-nosymbols-");
            break;
          case SymCoff:
            szSymType = _T("COFF");
            break;
          case SymCv:
            szSymType = _T("CV");
            break;
          case SymPdb:
            szSymType = _T("PDB");
            break;
          case SymExport:
            szSymType = _T("-exported-");
            break;
          case SymDeferred:
            szSymType = _T("-deferred-");
            break;
          case SymSym:
            szSymType = _T("SYM");
            break;
          case 8: //SymVirtual:
            szSymType = _T("Virtual");
            break;
          case 9: // SymDia:
            szSymType = _T("DIA");
            break;
        }
      }
      this->m_parent->OnLoadModule(img, mod, baseAddr, size, result, szSymType, Module.LoadedImageName, fileVersion);
    }
    if (szImg != NULL) free(szImg);
    if (szMod != NULL) free(szMod);
    return result;
  }
public:
  BOOL LoadModules(HANDLE hProcess, DWORD dwProcessId)
  {
    // first try toolhelp32
    if (GetModuleListTH32(hProcess, dwProcessId))
      return true;
    // then try psapi
    return GetModuleListPSAPI(hProcess);
  }

#ifdef _UNICODE
  BOOL GetModuleInfo(HANDLE hProcess, DWORD64 baseAddr, IMAGEHLP_MODULEW64 *pModuleInfo)
#else
  BOOL GetModuleInfo(HANDLE hProcess, DWORD64 baseAddr, IMAGEHLP_MODULE64 *pModuleInfo)
#endif
  {
    if(this->pSGMI == NULL)
    {
      SetLastError(ERROR_DLL_INIT_FAILED);
      return FALSE;
    }

    // as defined in VC7.1)...
#ifdef _UNICODE
    pModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULEW64);
#else
    pModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
#endif
    void *pData = malloc(4096); // reserve enough memory, so the bug in v6.3.5.1 does not lead to memory-overwrites...
    if (pData == NULL)
    {
      SetLastError(ERROR_NOT_ENOUGH_MEMORY);
      return FALSE;
    }
#ifdef _UNICODE
    memcpy(pData, pModuleInfo, sizeof(IMAGEHLP_MODULEW64));
    if (this->pSGMI(hProcess, baseAddr, (IMAGEHLP_MODULEW64*) pData) != FALSE)
#else
    memcpy(pData, pModuleInfo, sizeof(IMAGEHLP_MODULE64));
    if (this->pSGMI(hProcess, baseAddr, (IMAGEHLP_MODULE64*) pData) != FALSE)
#endif
    {
      // only copy as much memory as is reserved...
#ifdef _UNICODE
      memcpy(pModuleInfo, pData, sizeof(IMAGEHLP_MODULEW64));
      pModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULEW64);
#else
      memcpy(pModuleInfo, pData, sizeof(IMAGEHLP_MODULE64));
      pModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
#endif
      free(pData);
      return TRUE;
    }
    free(pData);
    SetLastError(ERROR_DLL_INIT_FAILED);
    return FALSE;
  }
};

// #############################################################
StackWalker::StackWalker(FILE * optOutFile, String * optOutString, int options, LPTSTR szSymPath, DWORD dwProcessId, HANDLE hProcess)
{
  this->m_outFile = optOutFile;
  this->m_outString = optOutString;
  this->m_options = options;
  this->m_modulesLoaded = FALSE;
  this->m_hProcess = hProcess;
  this->m_sw = new StackWalkerInternal(this, this->m_hProcess);
  this->m_dwProcessId = dwProcessId;
  if (szSymPath != NULL)
  {
    this->m_szSymPath = _tcsdup(szSymPath);
    this->m_options |= SymBuildPath;
  }
  else
    this->m_szSymPath = NULL;
}

StackWalker::~StackWalker()
{
  if (m_szSymPath != NULL)
    free(m_szSymPath);
  m_szSymPath = NULL;
  if (this->m_sw != NULL)
    delete this->m_sw;
  this->m_sw = NULL;
}

BOOL StackWalker::LoadModules()
{
  if (this->m_sw == NULL)
  {
    SetLastError(ERROR_DLL_INIT_FAILED);
    return FALSE;
  }
  if (m_modulesLoaded != FALSE)
    return TRUE;

  // Build the sym-path:
  TCHAR *szSymPath = NULL;
  if ( (this->m_options & SymBuildPath) != 0)
  {
    const size_t nSymPathLen = 4096;
    szSymPath = (TCHAR*) malloc(nSymPathLen * sizeof(TCHAR));
    if (szSymPath == NULL)
    {
      SetLastError(ERROR_NOT_ENOUGH_MEMORY);
      return FALSE;
    }
    szSymPath[0] = 0;
    // Now first add the (optional) provided sympath:
    if (this->m_szSymPath != NULL)
    {
      _tcscat_s(szSymPath, nSymPathLen, this->m_szSymPath);
      _tcscat_s(szSymPath, nSymPathLen, _T(";"));
    }

    _tcscat_s(szSymPath, nSymPathLen, _T(".;"));

    const size_t nTempLen = 1024;
    TCHAR szTemp[nTempLen];
    // Now add the current directory:
    if (GetCurrentDirectory(nTempLen, szTemp) > 0)
    {
      szTemp[nTempLen-1] = 0;
      _tcscat_s(szSymPath, nSymPathLen, szTemp);
      _tcscat_s(szSymPath, nSymPathLen, _T(";"));
    }

    // Now add the path for the main-module:
    if (GetModuleFileName(NULL, szTemp, nTempLen) > 0)
    {
      szTemp[nTempLen-1] = 0;
      for (TCHAR *p = (szTemp+_tcslen(szTemp)-1); p >= szTemp; --p)
      {
        // locate the rightmost path separator
        if ( (*p == '\\') || (*p == '/') || (*p == ':') )
        {
          *p = 0;
          break;
        }
      }  // for (search for path separator...)
      if (_tcslen(szTemp) > 0)
      {
        _tcscat_s(szSymPath, nSymPathLen, szTemp);
        _tcscat_s(szSymPath, nSymPathLen, _T(";"));
      }
    }
    if (GetEnvironmentVariable(_T("_NT_SYMBOL_PATH"), szTemp, nTempLen) > 0)
    {
      szTemp[nTempLen-1] = 0;
      _tcscat_s(szSymPath, nSymPathLen, szTemp);
      _tcscat_s(szSymPath, nSymPathLen, _T(";"));
    }
    if (GetEnvironmentVariable(_T("_NT_ALTERNATE_SYMBOL_PATH"), szTemp, nTempLen) > 0)
    {
      szTemp[nTempLen-1] = 0;
      _tcscat_s(szSymPath, nSymPathLen, szTemp);
      _tcscat_s(szSymPath, nSymPathLen, _T(";"));
    }
    if (GetEnvironmentVariable(_T("SYSTEMROOT"), szTemp, nTempLen) > 0)
    {
      szTemp[nTempLen-1] = 0;
      _tcscat_s(szSymPath, nSymPathLen, szTemp);
      _tcscat_s(szSymPath, nSymPathLen, _T(";"));
      // also add the "system32"-directory:
      _tcscat_s(szTemp, nTempLen, _T("\\system32"));
      _tcscat_s(szSymPath, nSymPathLen, szTemp);
      _tcscat_s(szSymPath, nSymPathLen, _T(";"));
    }

    if ( (this->m_options & SymBuildPath) != 0)
    {
      if (GetEnvironmentVariable(_T("SYSTEMDRIVE"), szTemp, nTempLen) > 0)
      {
        szTemp[nTempLen-1] = 0;
        _tcscat_s(szSymPath, nSymPathLen, _T("SRV*"));
        _tcscat_s(szSymPath, nSymPathLen, szTemp);
        _tcscat_s(szSymPath, nSymPathLen, _T("\\websymbols"));
        _tcscat_s(szSymPath, nSymPathLen, _T("*http://msdl.microsoft.com/download/symbols;"));
      }
      else
        _tcscat_s(szSymPath, nSymPathLen, _T("SRV*c:\\websymbols*http://msdl.microsoft.com/download/symbols;"));
    }
  }

  // First Init the whole stuff...
  BOOL bRet = this->m_sw->Init(szSymPath);
  if (szSymPath != NULL) free(szSymPath); szSymPath = NULL;
  if (bRet == FALSE)
  {
    this->OnDbgHelpErr(_T("Error while initializing dbghelp.dll"), 0, 0);
    SetLastError(ERROR_DLL_INIT_FAILED);
    return FALSE;
  }

  bRet = this->m_sw->LoadModules(this->m_hProcess, this->m_dwProcessId);
  if (bRet != FALSE)
    m_modulesLoaded = TRUE;
  return bRet;
}

static __declspec(align(16)) CONTEXT _context;
static int SaveContextFilterFunc(struct _EXCEPTION_POINTERS *ep) 
{
  memcpy_s(&_context, sizeof(CONTEXT), ep->ContextRecord, sizeof(CONTEXT));
  return EXCEPTION_EXECUTE_HANDLER;
}

// The following is used to pass the "userData"-Pointer to the user-provided readMemoryFunction
// This has to be done due to a problem with the "hProcess"-parameter in x64...
// Because this class is in no case multi-threading-enabled (because of the limitations
// of dbghelp.dll) it is "safe" to use a static-variable
static StackWalker::PReadProcessMemoryRoutine s_readMemoryFunction = NULL;
static LPVOID s_readMemoryFunction_UserData = NULL;

BOOL StackWalker::ShowCallstack(uint32 maxDepth, HANDLE hThread, const CONTEXT *context, PReadProcessMemoryRoutine readMemoryFunction, LPVOID pUserData)
{
#ifdef _UNICODE
  SYMBOL_INFOW *pSym = NULL;
  IMAGEHLP_MODULEW64 Module;
  IMAGEHLP_LINEW64 Line;
#else
  IMAGEHLP_SYMBOL64 *pSym = NULL;
  IMAGEHLP_MODULE64 Module;
  IMAGEHLP_LINE64 Line;
#endif

  if (m_modulesLoaded == FALSE)
    this->LoadModules();  // ignore the result...

  if (this->m_sw->m_hDbhHelp == NULL)
  {
    SetLastError(ERROR_DLL_INIT_FAILED);
    return FALSE;
  }

  s_readMemoryFunction = readMemoryFunction;
  s_readMemoryFunction_UserData = pUserData;

       if (context) _context = *context;
  else if (hThread == GetCurrentThread()) // If no context is provided, capture the context
  {
    __try
    {
       memset(&_context, 0, sizeof(CONTEXT));
       _context.ContextFlags = USED_CONTEXT_FLAGS;
       RtlCaptureContext(&_context);
    }
    __except(SaveContextFilterFunc(GetExceptionInformation()))
    {
       // Do nothing; the SaveContextFilterFunc() call has written to _context already (above)
    }
  }
  else
  {
    SuspendThread(hThread);
    memset(&_context, 0, sizeof(CONTEXT));
    _context.ContextFlags = USED_CONTEXT_FLAGS;
    if (GetThreadContext(hThread, &_context) == FALSE)
    {
      ResumeThread(hThread);
      return FALSE;
    }
  }

  // init STACKFRAME for first call
  STACKFRAME64 s; // in/out stackframe
  memset(&s, 0, sizeof(s));
  DWORD imageType;
#ifdef _M_IX86
  // normally, call ImageNtHeader() and use machine info from PE header
  imageType = IMAGE_FILE_MACHINE_I386;
  s.AddrPC.Offset = _context.Eip;
  s.AddrPC.Mode = AddrModeFlat;
  s.AddrFrame.Offset = _context.Ebp;
  s.AddrFrame.Mode = AddrModeFlat;
  s.AddrStack.Offset = _context.Esp;
  s.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
  imageType = IMAGE_FILE_MACHINE_AMD64;
  s.AddrPC.Offset = _context.Rip;
  s.AddrPC.Mode = AddrModeFlat;
  s.AddrFrame.Offset = _context.Rsp;
  s.AddrFrame.Mode = AddrModeFlat;
  s.AddrStack.Offset = _context.Rsp;
  s.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
  imageType = IMAGE_FILE_MACHINE_IA64;
  s.AddrPC.Offset = _context.StIIP;
  s.AddrPC.Mode = AddrModeFlat;
  s.AddrFrame.Offset = _context.IntSp;
  s.AddrFrame.Mode = AddrModeFlat;
  s.AddrBStore.Offset = _context.RsBSP;
  s.AddrBStore.Mode = AddrModeFlat;
  s.AddrStack.Offset = _context.IntSp;
  s.AddrStack.Mode = AddrModeFlat;
#elif _M_ARM
  imageType = IMAGE_FILE_MACHINE_ARM;
  s.AddrPC.Offset = _context.Pc;
  s.AddrPC.Mode = AddrModeFlat;
  s.AddrFrame.Offset = _context.R11;
  s.AddrFrame.Mode = AddrModeFlat;
  s.AddrStack.Offset = _context.Sp;
  s.AddrStack.Mode = AddrModeFlat;
#elif _M_ARM64
  imageType = IMAGE_FILE_MACHINE_ARM64;
  s.AddrPC.Offset = _context.Pc;
  s.AddrPC.Mode = AddrModeFlat;
  s.AddrFrame.Offset = _context.Fp;
  s.AddrFrame.Mode = AddrModeFlat;
  s.AddrStack.Offset = _context.Sp;
  s.AddrStack.Mode = AddrModeFlat;
#else
# error "StackWalker:  Platform not supported!"
#endif

#ifdef _UNICODE
  pSym = (SYMBOL_INFOW *)malloc(sizeof(SYMBOL_INFOW) + STACKWALK_MAX_NAMELEN);
#else
  pSym = (IMAGEHLP_SYMBOL64 *) malloc(sizeof(IMAGEHLP_SYMBOL64) + STACKWALK_MAX_NAMELEN);
#endif
  if (!pSym) goto cleanup;  // not enough memory...
#ifdef _UNICODE
  memset(pSym, 0, sizeof(SYMBOL_INFOW) + STACKWALK_MAX_NAMELEN);
  pSym->SizeOfStruct = sizeof(SYMBOL_INFOW);
  pSym->MaxNameLen = STACKWALK_MAX_NAMELEN;
#else
  memset(pSym, 0, sizeof(IMAGEHLP_SYMBOL64) + STACKWALK_MAX_NAMELEN);
  pSym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
  pSym->MaxNameLength = STACKWALK_MAX_NAMELEN;
#endif

  memset(&Line, 0, sizeof(Line));
  Line.SizeOfStruct = sizeof(Line);

  memset(&Module, 0, sizeof(Module));
  Module.SizeOfStruct = sizeof(Module);

  CallstackEntry *csEntry = newnothrow CallstackEntry;
  if (csEntry == NULL)
  {
    MWARN_OUT_OF_MEMORY;
    goto cleanup;
  }

  for (uint32 frameNum=0; frameNum<maxDepth; frameNum++)
  {
    // get next stack frame (StackWalk64(), SymFunctionTableAccess64(), SymGetModuleBase64())
    // if this returns ERROR_INVALID_ADDRESS (487) or ERROR_NOACCESS (998), you can
    // assume that either you are done, or that the stack is so hosed that the next
    // deeper frame could not be found.
    // CONTEXT need not to be supplied if imageType is IMAGE_FILE_MACHINE_I386!
    if ( ! this->m_sw->pSW(imageType, this->m_hProcess, hThread, &s, &_context, ReadProcMemCallback, this->m_sw->pSFTA, this->m_sw->pSGMB, NULL) )
    {
      this->OnDbgHelpErr(_T("StackWalk64"), GetLastError(), s.AddrPC.Offset);
      break;
    }

    csEntry->offset = s.AddrPC.Offset;
    csEntry->name[0] = 0;
    csEntry->undName[0] = 0;
    csEntry->undFullName[0] = 0;
    csEntry->offsetFromSymbol = 0;
    csEntry->offsetFromLine = 0;
    csEntry->lineFileName[0] = 0;
    csEntry->lineNumber = 0;
    csEntry->loadedImageName[0] = 0;
    csEntry->moduleName[0] = 0;
    if (s.AddrPC.Offset == s.AddrReturn.Offset)
    {
      this->OnDbgHelpErr(_T("StackWalk64-Endless-Callstack!"), 0, s.AddrPC.Offset);
      break;
    }
    if (s.AddrPC.Offset != 0)
    {
      // we seem to have a valid PC
      // show procedure info (SymGetSymFromAddr64())
#ifdef _UNICODE
      if (this->m_sw->pSFA(this->m_hProcess, s.AddrPC.Offset, &(csEntry->offsetFromSymbol), pSym) != FALSE)
#else
      if (this->m_sw->pSGSFA(this->m_hProcess, s.AddrPC.Offset, &(csEntry->offsetFromSymbol), pSym) != FALSE)
#endif
      {
        // TODO: Mache dies sicher...!
        _tcscpy_s(csEntry->name, pSym->Name);
        // UnDecorateSymbolName()
        this->m_sw->pUDSN( pSym->Name, csEntry->undName, STACKWALK_MAX_NAMELEN, UNDNAME_NAME_ONLY );
        this->m_sw->pUDSN( pSym->Name, csEntry->undFullName, STACKWALK_MAX_NAMELEN, UNDNAME_COMPLETE );
      }
      else
      {
#ifdef _UNICODE
        this->OnDbgHelpErr(_T("SymFromAddrW"), GetLastError(), s.AddrPC.Offset);
#else
        this->OnDbgHelpErr(_T("SymGetSymFromAddr64"), GetLastError(), s.AddrPC.Offset);
#endif
      }

      // show line number info, NT5.0-method (SymGetLineFromAddr64())
      if (this->m_sw->pSGLFA != NULL )
      { // yes, we have SymGetLineFromAddr64()
        if (this->m_sw->pSGLFA(this->m_hProcess, s.AddrPC.Offset, &(csEntry->offsetFromLine), &Line) != FALSE)
        {
          csEntry->lineNumber = Line.LineNumber;
          // TODO: Mache dies sicher...!
          _tcscpy_s(csEntry->lineFileName, Line.FileName);
        }
        else
        {
          this->OnDbgHelpErr(_T("SymGetLineFromAddr64"), GetLastError(), s.AddrPC.Offset);
        }
      } // yes, we have SymGetLineFromAddr64()

      // show module info (SymGetModuleInfo64())
      if (this->m_sw->GetModuleInfo(this->m_hProcess, s.AddrPC.Offset, &Module ) != FALSE)
      { // got module info OK
        switch ( Module.SymType )
        {
        case SymNone:
          csEntry->symTypeString = _T("-nosymbols-");
          break;
        case SymCoff:
          csEntry->symTypeString = _T("COFF");
          break;
        case SymCv:
          csEntry->symTypeString = _T("CV");
          break;
        case SymPdb:
          csEntry->symTypeString = _T("PDB");
          break;
        case SymExport:
          csEntry->symTypeString = _T("-exported-");
          break;
        case SymDeferred:
          csEntry->symTypeString = _T("-deferred-");
          break;
        case SymSym:
          csEntry->symTypeString = _T("SYM");
          break;
#if API_VERSION_NUMBER >= 9
        case SymDia:
          csEntry->symTypeString = _T("DIA");
          break;
#endif
        case 8: //SymVirtual:
          csEntry->symTypeString = _T("Virtual");
          break;
        default:
          csEntry->symTypeString = NULL;
          break;
        }

        // TODO: Mache dies sicher...!
        _tcscpy_s(csEntry->moduleName, Module.ModuleName);
        csEntry->baseOfImage = Module.BaseOfImage;
        _tcscpy_s(csEntry->loadedImageName, Module.LoadedImageName);
      } // got module info OK
      else
      {
        this->OnDbgHelpErr(_T("SymGetModuleInfo64"), GetLastError(), s.AddrPC.Offset);
      }
    } // we seem to have a valid PC

    CallstackEntryType et = (frameNum == 0) ? firstEntry : nextEntry;
    this->OnCallstackEntry(et, csEntry);

    if (s.AddrReturn.Offset == 0)
    {
      this->OnCallstackEntry(lastEntry, csEntry);
      SetLastError(ERROR_SUCCESS);
      break;
    }
  } // for ( frameNum )

  cleanup:
    if (csEntry) delete csEntry;
    if (pSym) free( pSym );

  if (context == NULL)
    ResumeThread(hThread);

  return TRUE;
}

BOOL __stdcall StackWalker::ReadProcMemCallback(
    HANDLE      hProcess,
    DWORD64     qwBaseAddress,
    PVOID       lpBuffer,
    DWORD       nSize,
    LPDWORD     lpNumberOfBytesRead
    )
{
  if (s_readMemoryFunction == NULL)
  {
    SIZE_T st = 0;
    BOOL bRet = ReadProcessMemory(hProcess, (LPVOID) qwBaseAddress, lpBuffer, nSize, &st);
    if (bRet)
      *lpNumberOfBytesRead = (DWORD) st;
    //printf("ReadMemory: hProcess: %p, baseAddr: %p, buffer: %p, size: %d, read: %d, result: %d\n", hProcess, (LPVOID) qwBaseAddress, lpBuffer, nSize, (DWORD) st, (DWORD) bRet);
    return bRet;
  }
  else
  {
    return s_readMemoryFunction(hProcess, qwBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead, s_readMemoryFunction_UserData);
  }
}

void StackWalker::OnLoadModule(LPTSTR img, LPTSTR mod, DWORD64 baseAddr, DWORD size, DWORD result, LPCTSTR symType, LPTSTR pdbName, ULONGLONG fileVersion)
{
   TCHAR buffer[STACKWALK_MAX_NAMELEN];
   if (fileVersion == 0) _sntprintf_s(buffer, STACKWALK_MAX_NAMELEN, _T("%s:%s (%p), size: %d (result: %d), SymType: '%s', PDB: '%s'\n"), img, mod, (LPVOID)baseAddr, size, result, symType, pdbName);
   else
   {
      DWORD v4 = (DWORD) fileVersion & 0xFFFF;
      DWORD v3 = (DWORD) (fileVersion>>16) & 0xFFFF;
      DWORD v2 = (DWORD) (fileVersion>>32) & 0xFFFF;
      DWORD v1 = (DWORD) (fileVersion>>48) & 0xFFFF;
      _sntprintf_s(buffer, STACKWALK_MAX_NAMELEN, _T("%s:%s (%p), size: %d (result: %d), SymType: '%s', PDB: '%s', fileVersion: %d.%d.%d.%d\n"), img, mod, (LPVOID)baseAddr, size, result, symType, pdbName, v1, v2, v3, v4);
   }
#ifdef REMOVED_BY_JAF_TOO_MUCH_INFORMATION
   OnOutput(buffer);
#endif
}

void StackWalker::OnCallstackEntry(CallstackEntryType eType, CallstackEntry *entry)
{
  TCHAR buffer[STACKWALK_MAX_NAMELEN];
  if ( (eType != lastEntry) && (entry->offset != 0) )
  {
    if (entry->name[0] == 0)
      _tcscpy_s(entry->name, _T("(function-name not available)"));
    if (entry->undName[0] != 0)
      _tcscpy_s(entry->name, entry->undName);
    if (entry->undFullName[0] != 0)
      _tcscpy_s(entry->name, entry->undFullName);
    if (entry->lineFileName[0] == 0)
    {
      if (entry->moduleName[0] == 0)
      {
        if (entry->loadedImageName[0] != 0)
           _sntprintf_s(buffer, STACKWALK_MAX_NAMELEN, _T("%s: %s+0x%I64x\n"), entry->loadedImageName, entry->name, (int64) entry->offsetFromSymbol);
         else
           _sntprintf_s(buffer, STACKWALK_MAX_NAMELEN, _T("%p: %s+0x%I64x\n"), (LPVOID)entry->offset, entry->name, (int64) entry->offsetFromSymbol);
      }
      else
      {
        if (entry->loadedImageName[0] != 0)
           _sntprintf_s(buffer, STACKWALK_MAX_NAMELEN, _T("%s (%s): %s+0x%I64x\n"), entry->loadedImageName, entry->moduleName, entry->name, (int64) entry->offsetFromSymbol);
         else
           _sntprintf_s(buffer, STACKWALK_MAX_NAMELEN, _T("%p (%s): %s+0x%I64x\n"), (LPVOID)entry->offset, entry->moduleName, entry->name, (int64) entry->offsetFromSymbol);
      }
    }
    else _sntprintf_s(buffer, STACKWALK_MAX_NAMELEN, _T("%s (%d): %s\n"), entry->lineFileName, entry->lineNumber, entry->name);

    OnOutput(buffer);
  }
}

void StackWalker::OnDbgHelpErr(LPCTSTR szFuncName, DWORD gle, DWORD64 addr)
{
  TCHAR buffer[STACKWALK_MAX_NAMELEN];
  _sntprintf_s(buffer, STACKWALK_MAX_NAMELEN, _T("ERROR: %s, GetLastError: %d (Address: %p)\n"), szFuncName, gle, (LPVOID)addr);
#ifdef REMOVED_BY_JAF_TOO_MUCH_INFORMATION
  OnOutput(buffer);
#elif _UNICODE
  size_t i;
  char buffer8[STACKWALK_MAX_NAMELEN];
  wcstombs_s(&i, buffer8, (size_t)STACKWALK_MAX_NAMELEN, buffer, (size_t)STACKWALK_MAX_NAMELEN );
  LogTime(MUSCLE_LOG_DEBUG, "%s", buffer8);
#else
  LogTime(MUSCLE_LOG_DEBUG, "%s", buffer);
#endif
}

void StackWalker::OnSymInit(LPTSTR szSearchPath, DWORD symOptions, LPTSTR szUserName)
{
#ifdef REMOVED_BY_JAF_TOO_MUCH_INFORMATION
  TCHAR buffer[STACKWALK_MAX_NAMELEN];
  _sntprintf_s(buffer, STACKWALK_MAX_NAMELEN, _T("SymInit: Symbol-SearchPath: '%s', symOptions: %d, UserName: '%s'\n"), szSearchPath, symOptions, szUserName);
  OnOutput(buffer);

  // Also display the OS-version
  OSVERSIONINFOEX ver; ZeroMemory(&ver, sizeof(OSVERSIONINFOEX));
  ver.dwOSVersionInfoSize = sizeof(ver);
  if (GetVersionEx( (OSVERSIONINFO*) &ver) != FALSE)   // Note:  this call is kind of useless under Win8 and higher since Win8 lies to us :(
  {
    _sntprintf_s(buffer, STACKWALK_MAX_NAMELEN, _T("OS-Version: %d.%d.%d (%s) 0x%x-0x%x\n"), ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber, ver.szCSDVersion, ver.wSuiteMask, ver.wProductType);
    OnOutput(buffer);
  }
#else
  (void) szSearchPath;
  (void) symOptions;
  (void) szUserName;
#endif
}

#endif  // Windows stack trace code

// Win32 doesn't have localtime_r, so we have to roll our own
#if defined(WIN32)
static inline struct tm * muscle_localtime_r(time_t * clock, struct tm * result)
{
   // Note that in Win32, (ret) points to thread-local storage, so this really
   // is thread-safe despite the fact that it looks like it isn't!
#if __STDC_WANT_SECURE_LIB__
   (void) localtime_s(result, clock);
   return result;
#else
   struct tm temp;
   struct tm * ret = localtime_r(clock, &temp);
   if (ret) *result = *ret;
   return ret;
#endif
}
static inline struct tm * muscle_gmtime_r(time_t * clock, struct tm * result)
{
   // Note that in Win32, (ret) points to thread-local storage, so this really
   // is thread-safe despite the fact that it looks like it isn't!
#if __STDC_WANT_SECURE_LIB__
   (void) gmtime_s(result, clock);
   return result;
#else
   struct tm temp;
   struct tm * ret = gmtime_r(clock, &temp);
   if (ret) *result = *ret;
   return ret;
#endif
}
#else
static inline struct tm * muscle_localtime_r(time_t * clock, struct tm * result) {return localtime_r(clock, result);}
static inline struct tm * muscle_gmtime_r(   time_t * clock, struct tm * result) {return gmtime_r(clock, result);}
#endif

#ifndef MUSCLE_INLINE_LOGGING

#define MAX_STACK_TRACE_DEPTH ((uint32)(256))

status_t PrintStackTrace(FILE * optFile, uint32 maxDepth)
{
   TCHECKPOINT;

   if (optFile == NULL) optFile = stdout;

#if defined(MUSCLE_USE_BACKTRACE)
   void *array[MAX_STACK_TRACE_DEPTH];
   const size_t size = backtrace(array, muscleMin(maxDepth, MAX_STACK_TRACE_DEPTH));
   fprintf(optFile, "--Stack trace follows (%i frames):\n", (int) size);
   backtrace_symbols_fd(array, (int)size, fileno(optFile));
   fprintf(optFile, "--End Stack trace\n");
   return B_NO_ERROR;
#elif defined(MUSCLE_USE_MSVC_STACKWALKER)
   _Win32PrintStackTraceForContext(optFile, NULL, maxDepth);
   return B_NO_ERROR;
#else
   (void) maxDepth;  // shut the compiler up
   fprintf(optFile, "PrintStackTrace:  Error, stack trace printing not available on this platform!\n");
   return B_UNIMPLEMENTED;  // I don't know how to do this for other systems!
#endif
}

status_t GetStackTrace(String & retStr, uint32 maxDepth)
{
   TCHECKPOINT;

#if defined(MUSCLE_USE_BACKTRACE)
   void *array[MAX_STACK_TRACE_DEPTH];
   const size_t size = backtrace(array, muscleMin(maxDepth, MAX_STACK_TRACE_DEPTH));
   char ** strings = backtrace_symbols(array, (int)size);
   if (strings)
   {
      char buf[128];
      muscleSprintf(buf, "--Stack trace follows (%zd frames):", size); retStr += buf;
      for (size_t i = 0; i < size; i++)
      {
         retStr += "\n  ";
         retStr += strings[i];
      }
      retStr += "\n--End Stack trace\n";
      free(strings);
      return B_NO_ERROR;
   }
   else return B_OUT_OF_MEMORY;
#elif defined(MUSCLE_USE_MSVC_STACKWALKER)
   StackWalker(NULL, &retStr, StackWalker::OptionsJAF).ShowCallstack(maxDepth);
   return B_NO_ERROR;
#else
   (void) retStr;   // shut the compiler up
   (void) maxDepth;
   return B_UNIMPLEMENTED;
#endif
}

#ifdef MUSCLE_RECORD_REFCOUNTABLE_ALLOCATION_LOCATIONS
void UpdateAllocationStackTrace(bool isAllocation, String * & s)
{
   if (isAllocation)
   {
      if (s == NULL) 
      {
         s = newnothrow String;
         if (s == NULL) MWARN_OUT_OF_MEMORY;
      }
      if (s)
      {
         if (GetStackTrace(*s).IsError()) s->SetCstr("(no stack trace available)");
      }
   }
   else
   {
      delete s;
      s = NULL;
   }
}

void PrintAllocationStackTrace(const void * slabThis, const void * obj, uint32 slabIdx, uint32 numObjectsPerSlab, const String & stackStr)
{
   printf("\nObjectSlab %p:  Object %p (#" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ") was allocated at this location:\n", slabThis, obj, slabIdx, numObjectsPerSlab);
   puts(stackStr());
}
#endif

static NestCount _inLogPreamble;

static const char * const _logLevelNames[] = {
   "None",
   "Critical Errors Only",
   "Errors Only",
   "Warnings and Errors Only",
   "Informational",
   "Debug",
   "Trace"
};

static const char * const _logLevelKeywords[] = {
   "none",
   "critical",
   "errors",
   "warnings",
   "info",
   "debug",
   "trace"
};

DefaultConsoleLogger :: DefaultConsoleLogger()
   : _consoleLogLevel(MUSCLE_LOG_INFO)
{
    // empty
}

void DefaultConsoleLogger :: Log(const LogCallbackArgs & a)
{
   if (a.GetLogLevel() <= _consoleLogLevel)
   {
      vprintf(a.GetText(), *a.GetArgList());
      fflush(stdout);
   }
}

void DefaultConsoleLogger :: Flush()
{
   fflush(stdout);
}

DefaultFileLogger :: DefaultFileLogger()
   : _fileLogLevel(MUSCLE_LOG_NONE)
   , _maxLogFileSize(MUSCLE_NO_LIMIT)
   , _maxNumLogFiles(MUSCLE_NO_LIMIT)
   , _compressionEnabled(false)
   , _logFileOpenAttemptFailed(false)
{
   // empty
}

DefaultFileLogger :: ~DefaultFileLogger()
{
   CloseLogFile();
}

void DefaultFileLogger :: Log(const LogCallbackArgs & a)
{
   if ((a.GetLogLevel() <= GetFileLogLevel())&&(EnsureLogFileCreated(a).IsOK()))
   {
      vfprintf(_logFile.GetFile(), a.GetText(), *a.GetArgList());
      _logFile.FlushOutput();
      if ((_maxLogFileSize != MUSCLE_NO_LIMIT)&&(_inLogPreamble.IsInBatch() == false))  // wait until we're outside the preamble to avoid breaking up lines too much
      {
         const int64 curFileSize = _logFile.GetPosition();
         if ((curFileSize < 0)||(curFileSize >= (int64)_maxLogFileSize))
         {
            const uint32 tempStoreSize = _maxLogFileSize;
            _maxLogFileSize = MUSCLE_NO_LIMIT;  // otherwise we'd recurse indefinitely here!
            CloseLogFile();
            _maxLogFileSize = tempStoreSize;
            (void) EnsureLogFileCreated(a);  // force the opening of the new log file right now, so that the open message show up in the right order
         }
      }
   }
}

void DefaultFileLogger :: Flush()
{
   _logFile.FlushOutput();
}

uint32 DefaultFileLogger :: AddPreExistingLogFiles(const String & filePattern)
{
   String dirPart, filePart;
   const int32 lastSlash = filePattern.LastIndexOf(GetFilePathSeparator());
   if (lastSlash >= 0)
   {
      dirPart = filePattern.Substring(0, lastSlash);
      filePart = filePattern.Substring(lastSlash+1);
   }
   else
   {
      dirPart  = ".";
      filePart = filePattern;
   }

   Hashtable<String, uint64> pathToTime;
   if (filePart.HasChars())
   {
      StringMatcher sm(filePart);

      Directory d(dirPart());
      if (d.IsValid())
      {
         const char * nextName;
         while((nextName = d.GetCurrentFileName()) != NULL)
         {
            const String fn = nextName;
            if (sm.Match(fn))
            {
               const String fullPath = dirPart+GetFilePathSeparator()+fn;
               FilePathInfo fpi(fullPath());
               if ((fpi.Exists())&&(fpi.IsRegularFile())) pathToTime.Put(fullPath, fpi.GetCreationTime());
            }
            d++;
         }
      }

      // Now we sort by creation time...
      pathToTime.SortByValue();

      // And add the results to our _oldFileNames queue.  That way when the log file is opened, the oldest files will be deleted (if appropriate)
      for (HashtableIterator<String, uint64> iter(pathToTime); iter.HasData(); iter++) (void) _oldLogFileNames.AddTail(iter.GetKey());
   }
   return pathToTime.GetNumItems();
}

status_t DefaultFileLogger :: EnsureLogFileCreated(const LogCallbackArgs & a)
{
   if ((_logFile.GetFile() == NULL)&&(_logFileOpenAttemptFailed == false))
   {
      String logFileName = _prototypeLogFileName;
      if (logFileName.IsEmpty()) logFileName = "%f.log";

      HumanReadableTimeValues hrtv; (void) GetHumanReadableTimeValues(SecondsToMicros(a.GetWhen()), hrtv);
      logFileName = hrtv.ExpandTokens(logFileName);

      _logFile.SetFile(muscleFopen(logFileName(), "w"));
      if (_logFile.GetFile() != NULL)
      {
         _activeLogFileName = logFileName;
         LogTime(MUSCLE_LOG_DEBUG, "Created Log file [%s]\n", _activeLogFileName());

         while(_oldLogFileNames.GetNumItems() >= _maxNumLogFiles)
         {
            const char * c = _oldLogFileNames.Head()();
                 if (remove(c) == 0)  LogTime(MUSCLE_LOG_DEBUG, "Deleted old Log file [%s]\n", c);
            else if (errno != ENOENT) LogTime(MUSCLE_LOG_ERROR, "Error [%s] deleting old Log file [%s]\n", B_ERRNO(), c);
            _oldLogFileNames.RemoveHead();
         }

         const String headerString = GetLogFileHeaderString(a);
         if (headerString.HasChars()) fprintf(_logFile.GetFile(), "%s\n", headerString());
      }
      else
      {
         _activeLogFileName.Clear();
         _logFileOpenAttemptFailed = true;  // avoid an indefinite number of log-failed messages
         LogTime(MUSCLE_LOG_ERROR, "Failed to open Log file [%s], logging to file is now disabled. [%s]\n", logFileName(), B_ERRNO());
      }
   }
   return (_logFile.GetFile() != NULL) ? B_NO_ERROR : B_IO_ERROR;
}

void DefaultFileLogger :: CloseLogFile()
{
   if (_logFile.GetFile())
   {
      LogTime(MUSCLE_LOG_DEBUG, "Closing Log file [%s]\n", _activeLogFileName());
      String oldFileName = _activeLogFileName;  // default file to delete later, will be changed if/when we've made the .gz file
      _activeLogFileName.Clear();   // do this first to avoid reentrancy issues
      _logFile.Shutdown();

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
      if (_compressionEnabled)
      {
         FileDataIO inIO(muscleFopen(oldFileName(), "rb"));
         if (inIO.GetFile() != NULL)
         {
            const String gzName = oldFileName + ".gz";
            gzFile gzOut = gzopen(gzName(), "wb9"); // 9 for maximum compression
            if (gzOut != Z_NULL)
            {
               bool ok = true;

               const uint32 bufSize = 128*1024;
               char * buf = newnothrow char[bufSize];
               if (buf)
               {
                  while(1)
                  {
                     const int32 bytesRead = inIO.Read(buf, bufSize);
                     if (bytesRead < 0) break;  // EOF

                     const int bytesWritten = gzwrite(gzOut, buf, bytesRead);
                     if (bytesWritten <= 0)
                     {
                        ok = false;  // write error, oh dear
                        break;
                     }
                  }
                  delete [] buf;
               }
               else MWARN_OUT_OF_MEMORY;

               gzclose(gzOut);

               if (ok)
               {
                  inIO.Shutdown();
                  if (remove(oldFileName()) != 0) LogTime(MUSCLE_LOG_ERROR, "Error deleting log file [%s] after compressing it to [%s] [%s]!\n", oldFileName(), gzName(), B_ERRNO());
                  oldFileName = gzName;
               }
               else
               {
                  if (remove(gzName()) != 0) LogTime(MUSCLE_LOG_ERROR, "Error deleting gzip'd log file [%s] after compression failed! [%s]\n", gzName(), B_ERRNO());
               }
            }
            else LogTime(MUSCLE_LOG_ERROR, "Could not open compressed Log file [%s]! [%s]\n", gzName(), B_ERRNO());
         }
         else LogTime(MUSCLE_LOG_ERROR, "Could not reopen Log file [%s] to compress it! [%s]\n", oldFileName(), B_ERRNO());
      }
#endif
      if ((_maxNumLogFiles != MUSCLE_NO_LIMIT)&&(_oldLogFileNames.Contains(oldFileName) == false)) (void) _oldLogFileNames.AddTail(oldFileName);  // so we can delete it later
   }
}

LogLineCallback :: LogLineCallback()
   : _writeTo(_buf)
{
   _buf[0] = '\0';
   _buf[sizeof(_buf)-1] = '\0';  // just in case vsnsprintf() has to truncate
}

LogLineCallback :: ~LogLineCallback()
{
   // empty
}

void LogLineCallback :: Log(const LogCallbackArgs & a)
{
   TCHECKPOINT;

   // Generate the new text
#ifdef __MWERKS__
   const int bytesAttempted = vsprintf(_writeTo, a.GetText(), *a.GetArgList());  // BeOS/PPC doesn't know vsnprintf :^P
#elif __STDC_WANT_SECURE_LIB__
   const int bytesAttempted = _vsnprintf_s(_writeTo, (sizeof(_buf)-1)-(_writeTo-_buf), _TRUNCATE, a.GetText(), *a.GetArgList());  // the -1 is for the guaranteed NUL terminator
#elif WIN32
   const int bytesAttempted = _vsnprintf(_writeTo, (sizeof(_buf)-1)-(_writeTo-_buf), a.GetText(), *a.GetArgList());  // the -1 is for the guaranteed NUL terminator
#else
   const int bytesAttempted = vsnprintf(_writeTo, (sizeof(_buf)-1)-(_writeTo-_buf), a.GetText(), *a.GetArgList());  // the -1 is for the guaranteed NUL terminator
#endif
   const bool wasTruncated = (bytesAttempted != (int)strlen(_writeTo));  // do not combine with above line!

   // Log any newly completed lines
   char * logFrom  = _buf;
   char * searchAt = _writeTo;
   LogCallbackArgs tmp(a);
   while(true)
   {
      char * nextReturn = strchr(searchAt, '\n');
      if (nextReturn)
      {
         *nextReturn = '\0';  // terminate the string
         tmp.SetText(logFrom);
         LogLine(tmp);
         searchAt = logFrom = nextReturn+1;
      }
      else
      {
         // If we ran out of buffer space and no carriage returns were detected,
         // then we need to just dump what we have and move on, there's nothing else we can do
         if (wasTruncated)
         {
            tmp.SetText(logFrom);
            LogLine(tmp);
            _buf[0] = '\0';
            _writeTo = searchAt = logFrom = _buf;
         }
         break;
      }
   }

   // And finally, move any remaining incomplete lines back to the beginning of the array, for next time
   if (logFrom > _buf)
   {
      const int slen = (int) strlen(logFrom);
      memmove(_buf, logFrom, slen+1);  // include NUL byte
      _writeTo = &_buf[slen];          // point to our just-moved NUL byte
   }
   else _writeTo = strchr(searchAt, '\0');

   _lastLog = a;
}

void LogLineCallback :: Flush()
{
   TCHECKPOINT;

   if (_writeTo > _buf)
   {
      _lastLog.SetText(_buf);
      LogLine(_lastLog);
      _writeTo = _buf;
      _buf[0] = '\0';
   }
}

static Mutex _logMutex;
static Hashtable<LogCallbackRef, Void> _logCallbacks;
static DefaultConsoleLogger _dcl;
static DefaultFileLogger _dfl;

status_t LockLog()
{
   return _logMutex.Lock();
}

status_t UnlockLog()
{
   return _logMutex.Unlock();
}

const char * GetLogLevelName(int ll)
{
   return ((ll>=0)&&(ll<(int) ARRAYITEMS(_logLevelNames))) ? _logLevelNames[ll] : "???";
}

const char * GetLogLevelKeyword(int ll)
{
   return ((ll>=0)&&(ll<(int) ARRAYITEMS(_logLevelKeywords))) ? _logLevelKeywords[ll] : "???";
}

int ParseLogLevelKeyword(const char * keyword)
{
   for (uint32 i=0; i<ARRAYITEMS(_logLevelKeywords); i++) if (strcmp(keyword, _logLevelKeywords[i]) == 0) return i;
   return -1;
}

int GetFileLogLevel()
{
   return _dfl.GetFileLogLevel();
}

String GetFileLogName()
{
   return _dfl.GetFileLogName();
}

uint32 GetFileLogMaximumSize()
{
   return _dfl.GetMaxLogFileSize();
}

uint32 GetMaxNumLogFiles()
{
   return _dfl.GetMaxNumLogFiles();
}

bool GetFileLogCompressionEnabled()
{
   return _dfl.GetFileCompressionEnabled();
}

int GetConsoleLogLevel()
{
   return _dcl.GetConsoleLogLevel();
}

int GetMaxLogLevel()
{
   return muscleMax(_dcl.GetConsoleLogLevel(), _dfl.GetFileLogLevel());
}

status_t SetFileLogName(const String & logName)
{
   status_t ret;
   if (LockLog().IsOK(ret))
   {
      _dfl.SetLogFileName(logName);
      LogTime(MUSCLE_LOG_DEBUG, "File log name set to: %s\n", logName());
      (void) UnlockLog();
      return B_NO_ERROR;
   }
   else return ret;
}

status_t SetOldLogFilesPattern(const String & pattern)
{
   status_t ret;
   if (LockLog().IsOK(ret))
   {
      const uint32 numAdded = _dfl.AddPreExistingLogFiles(pattern);
      LogTime(MUSCLE_LOG_DEBUG, "Old Log Files pattern set to: [%s] (" UINT32_FORMAT_SPEC " files matched)\n", pattern(), numAdded);
      (void) UnlockLog();
      return B_NO_ERROR;
   }
   else return ret;
}

status_t SetFileLogMaximumSize(uint32 maxSizeBytes)
{
   status_t ret;
   if (LockLog().IsOK(ret))
   {
      _dfl.SetMaxLogFileSize(maxSizeBytes);
      if (maxSizeBytes == MUSCLE_NO_LIMIT) LogTime(MUSCLE_LOG_DEBUG, "File log maximum size set to: (unlimited).\n");
                                      else LogTime(MUSCLE_LOG_DEBUG, "File log maximum size set to: " UINT32_FORMAT_SPEC " bytes.\n", maxSizeBytes);
      (void) UnlockLog();
      return B_NO_ERROR;
   }
   else return ret;
}

status_t SetMaxNumLogFiles(uint32 maxNumLogFiles)
{
   status_t ret;
   if (LockLog().IsOK(ret))
   {
      _dfl.SetMaxNumLogFiles(maxNumLogFiles);
      if (maxNumLogFiles == MUSCLE_NO_LIMIT) LogTime(MUSCLE_LOG_DEBUG, "Maximum number of log files set to: (unlimited).\n");
                                        else LogTime(MUSCLE_LOG_DEBUG, "Maximum number of log files to: " UINT32_FORMAT_SPEC "\n", maxNumLogFiles);
      (void) UnlockLog();
      return B_NO_ERROR;
   }
   else return ret;
}

status_t SetFileLogCompressionEnabled(bool enable)
{
#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
   status_t ret;
   if (LockLog().IsOK(ret))
   {
      _dfl.SetFileCompressionEnabled(enable);
      LogTime(MUSCLE_LOG_DEBUG, "File log compression %s.\n", enable?"enabled":"disabled");
      (void) UnlockLog();
      return B_NO_ERROR;
   }
   else return ret;
#else
   if (enable)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Can not enable log file compression, MUSCLE was compiled without MUSCLE_ENABLE_ZLIB_ENCODING specified!\n");
      return B_UNIMPLEMENTED;
   }
   else return B_NO_ERROR;
#endif
}

void CloseCurrentLogFile()
{
   status_t ret;
   if (LockLog().IsOK(ret))
   {
      _dfl.CloseLogFile();
      (void) UnlockLog();
   }
}

status_t SetFileLogLevel(int loglevel)
{
   status_t ret;
   if (LockLog().IsOK(ret))
   {
      _dfl.SetFileLogLevel(loglevel);
      LogTime(MUSCLE_LOG_DEBUG, "File logging level set to: %s\n", GetLogLevelName(_dfl.GetFileLogLevel()));
      (void) UnlockLog();
      return B_NO_ERROR;
   }
   else return ret;
}

status_t SetConsoleLogLevel(int loglevel)
{
   status_t ret;
   if (LockLog().IsOK(ret))
   {
      _dcl.SetConsoleLogLevel(loglevel);
      LogTime(MUSCLE_LOG_DEBUG, "Console logging level set to: %s\n", GetLogLevelName(_dcl.GetConsoleLogLevel()));
      (void) UnlockLog();
      return B_NO_ERROR;
   }
   else return ret;
}

// Our 26-character alphabet of usable symbols
#define NUM_CHARS_IN_KEY_ALPHABET (sizeof(_keyAlphabet)-1)  // -1 because the NUL terminator doesn't count
static const char _keyAlphabet[] = "2346789BCDFGHJKMNPRSTVWXYZ";  // FogBugz #5808: vowels and some numerals omitted to avoid ambiguity and inadvertent swearing
static const uint32 _keySpaceSize = NUM_CHARS_IN_KEY_ALPHABET * NUM_CHARS_IN_KEY_ALPHABET * NUM_CHARS_IN_KEY_ALPHABET * NUM_CHARS_IN_KEY_ALPHABET;

uint32 GenerateSourceCodeLocationKey(const char * fileName, uint32 lineNumber)
{
#ifdef WIN32
   const char * lastSlash = strrchr(fileName, '\\');
#else
   const char * lastSlash = strrchr(fileName, '/');
#endif
   if (lastSlash) fileName = lastSlash+1;

   return ((CalculateHashCode(fileName,(uint32)strlen(fileName))+lineNumber)%(_keySpaceSize-1))+1;  // note that 0 is not considered a valid key value!
}

String SourceCodeLocationKeyToString(uint32 key)
{
   if (key == 0) return "";                  // 0 is not a valid key value
   if (key >= _keySpaceSize) return "????";  // values greater than or equal to our key space size are errors

   char buf[5]; buf[4] = '\0';
   for (int32 i=3; i>=0; i--)
   {
      buf[i] = _keyAlphabet[key % NUM_CHARS_IN_KEY_ALPHABET];
      key /= NUM_CHARS_IN_KEY_ALPHABET;
   }
   return buf;
}

uint32 SourceCodeLocationKeyFromString(const String & ss)
{
   String s = ss.ToUpperCase().Trim();
   if (s.Length() != 4) return 0;  // codes must always be exactly 4 characters long!

   s.Replace('0', 'O');
   s.Replace('1', 'I');
   s.Replace('5', 'S');

   uint32 ret  = 0;
   uint32 base = 1;
   for (int32 i=3; i>=0; i--)
   {
      const char * p = strchr(_keyAlphabet, s[i]);
      if (p == NULL) return 0;  // invalid character!

      const int whichChar = (int) (p-_keyAlphabet);
      ret += (whichChar*base);
      base *= NUM_CHARS_IN_KEY_ALPHABET;
   }
   return ret;
}

void GetStandardLogLinePreamble(char * buf, const LogCallbackArgs & a)
{
   const size_t MINIMUM_PREMABLE_BUF_SIZE_PER_DOCUMENTATION = 64;
   struct tm ltm;
   time_t when = a.GetWhen();
   struct tm * temp = muscle_localtime_r(&when, &ltm);
#ifdef MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME
# ifdef MUSCLE_LOG_VERBOSE_SOURCE_LOCATIONS
   const char * fn = a.GetSourceFile();
   const char * lastSlash = fn ? strrchr(fn, '/') : NULL;
#  ifdef WIN32
   const char * lastBackSlash = fn ? strrchr(fn, '\\') : NULL;
   if ((lastBackSlash)&&((lastSlash == NULL)||(lastBackSlash > lastSlash))) lastSlash = lastBackSlash;
#  endif
   if (lastSlash) fn = lastSlash+1;

   static const size_t suffixSize = 16;
   muscleSnprintf(buf, MINIMUM_PREMABLE_BUF_SIZE_PER_DOCUMENTATION-suffixSize, "[%c %02i/%02i %02i:%02i:%02i] [%s", GetLogLevelName(a.GetLogLevel())[0], temp->tm_mon+1, temp->tm_mday, temp->tm_hour, temp->tm_min, temp->tm_sec, fn);
   char buf2[suffixSize];
   muscleSnprintf(buf2, sizeof(buf2), ":%i] ", a.GetSourceLineNumber());
   strncat(buf, buf2, MINIMUM_PREMABLE_BUF_SIZE_PER_DOCUMENTATION);
# else
   muscleSnprintf(buf, MINIMUM_PREMABLE_BUF_SIZE_PER_DOCUMENTATION, "[%c %02i/%02i %02i:%02i:%02i] [%s] ", GetLogLevelName(a.GetLogLevel())[0], temp->tm_mon+1, temp->tm_mday, temp->tm_hour, temp->tm_min, temp->tm_sec, SourceCodeLocationKeyToString(GenerateSourceCodeLocationKey(a.GetSourceFile(), a.GetSourceLineNumber()))());
#endif
#else
   muscleSnprintf(buf, MINIMUM_PREMABLE_BUF_SIZE_PER_DOCUMENTATION, "[%c %02i/%02i %02i:%02i:%02i] ", GetLogLevelName(a.GetLogLevel())[0], temp->tm_mon+1, temp->tm_mday, temp->tm_hour, temp->tm_min, temp->tm_sec);
#endif
}

static NestCount _inWarnOutOfMemory;  // avoid potential infinite recursion if we are logging out-of-memory errors but our LogCallbacks try to allocate memory to do the log operation :^P

#define DO_LOGGING_CALLBACK(cb) \
{                               \
   va_list argList;             \
   va_start(argList, fmt);      \
   cb.Log(LogCallbackArgs(when, ll, sourceFile, sourceFunction, sourceLine, fmt, &argList)); \
   va_end(argList);             \
}

#define DO_LOGGING_CALLBACKS for (HashtableIterator<LogCallbackRef, Void> iter(_logCallbacks); iter.HasData(); iter++) if (iter.GetKey()()) DO_LOGGING_CALLBACK((*iter.GetKey()()));

#ifdef MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME
status_t _LogTime(const char * sourceFile, const char * sourceFunction, int sourceLine, int ll, const char * fmt, ...)
#else
status_t LogTime(int ll, const char * fmt, ...)
#endif
{
   const status_t lockRet = LockLog();
   if (_inWarnOutOfMemory.GetCount() < 2)  // avoid potential infinite recursion (while still allowing the first Out-of-memory message to attempt to get into the log)
   {
      // First, log the preamble
      const time_t when = time(NULL);
      char buf[128];

#ifndef MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME
      static const char * sourceFile     = "";
      static const char * sourceFunction = "";
      static const int sourceLine        = -1;
#endif

      // First, send to the log file
      {
         NestCountGuard g(_inLogPreamble);  // must be inside the braces!
         va_list dummyList;
         va_start(dummyList, fmt);  // not used
         LogCallbackArgs lca(when, ll, sourceFile, sourceFunction, sourceLine, buf, &dummyList);
         GetStandardLogLinePreamble(buf, lca);
         lca.SetText(buf);
         _dfl.Log(lca);
         va_end(dummyList);
      }
      DO_LOGGING_CALLBACK(_dfl);

      // Then, send to the display
      {
         NestCountGuard g(_inLogPreamble);  // must be inside the braces!
         va_list dummyList;
         va_start(dummyList, fmt);  // not used
         _dcl.Log(LogCallbackArgs(when, ll, sourceFile, sourceFunction, sourceLine, buf, &dummyList));
         va_end(dummyList);
      }
      DO_LOGGING_CALLBACK(_dcl);  // must be outside of the braces!

      // Then log the actual message as supplied by the user
      if (lockRet.IsOK()) DO_LOGGING_CALLBACKS;
   }
   if (lockRet.IsOK()) UnlockLog();

   return lockRet;
}

status_t LogFlush()
{
   TCHECKPOINT;

   status_t ret;
   if (LockLog().IsOK(ret))
   {
      for (HashtableIterator<LogCallbackRef, Void> iter(_logCallbacks); iter.HasData(); iter++) if (iter.GetKey()()) iter.GetKey()()->Flush();
      (void) UnlockLog();
      return B_NO_ERROR;
   }
   else return ret;
}

status_t LogStackTrace(int ll, uint32 maxDepth)
{
   TCHECKPOINT;

#if defined(MUSCLE_USE_BACKTRACE)
   void *array[MAX_STACK_TRACE_DEPTH];
   const size_t size = backtrace(array, muscleMin(maxDepth, MAX_STACK_TRACE_DEPTH));
   char ** strings = backtrace_symbols(array, (int)size);
   if (strings)
   {
      LogTime(ll, "--Stack trace follows (%zd frames):\n", size);
      for (size_t i = 0; i < size; i++) LogTime(ll, "  %s\n", strings[i]);
      LogTime(ll, "--End Stack trace\n");
      free(strings);
      return B_NO_ERROR;
   }
   else return B_OUT_OF_MEMORY;
#else
   (void) ll;        // shut the compiler up
   (void) maxDepth;  // shut the compiler up
   return B_UNIMPLEMENTED;  // I don't know how to do this for other systems!
#endif
}

status_t Log(int ll, const char * fmt, ...)
{
   const status_t lockRet = LockLog();
   {
      // No way to get these, since #define Log() as a macro causes
      // nasty namespace collisions with other methods/functions named Log()
      static const char * sourceFile     = "";
      static const char * sourceFunction = "";
      static const int sourceLine        = -1;

      const time_t when = time(NULL);  // don't inline this, ya dummy
      DO_LOGGING_CALLBACK(_dfl);
      DO_LOGGING_CALLBACK(_dcl);
      if (lockRet.IsOK()) DO_LOGGING_CALLBACKS;
   }
   if (lockRet.IsOK()) (void) UnlockLog();
   return lockRet;
}

status_t PutLogCallback(const LogCallbackRef & cb)
{
   status_t ret;
   if (LockLog().IsOK(ret))
   {
      ret = _logCallbacks.PutWithDefault(cb);
      (void) UnlockLog();
   }
   return ret;
}

status_t ClearLogCallbacks()
{
   status_t ret;
   if (LockLog().IsOK(ret))
   {
      _logCallbacks.Clear();
      (void) UnlockLog();
   }
   return ret;
}

status_t RemoveLogCallback(const LogCallbackRef & cb)
{
   status_t ret;
   if (LockLog().IsOK(ret))
   {
      ret = _logCallbacks.Remove(cb);
      (void) UnlockLog();
   }
   return ret;
}

#endif

#ifdef WIN32
static const uint64 _windowsDiffTime = ((uint64)116444736)*NANOS_PER_SECOND; // add (1970-1601) to convert to Windows time base
#endif

status_t GetHumanReadableTimeValues(uint64 timeUS, HumanReadableTimeValues & v, uint32 timeType)
{
   TCHECKPOINT;

   if (timeUS == MUSCLE_TIME_NEVER) return B_BAD_ARGUMENT;

   const int microsLeft = (int)(timeUS % MICROS_PER_SECOND);

#ifdef WIN32
   // Borland's localtime() function is buggy, so we'll use the Win32 API instead.
   const uint64 winTime = (timeUS*10) + _windowsDiffTime;  // Convert to (100ns units)

   FILETIME fileTime;
   fileTime.dwHighDateTime = (DWORD) ((winTime>>32) & 0xFFFFFFFF);
   fileTime.dwLowDateTime  = (DWORD) ((winTime>> 0) & 0xFFFFFFFF);

   SYSTEMTIME st;
   if (FileTimeToSystemTime(&fileTime, &st))
   {
      if (timeType == MUSCLE_TIMEZONE_UTC)
      {
         TIME_ZONE_INFORMATION tzi;
         if ((GetTimeZoneInformation(&tzi) == TIME_ZONE_ID_INVALID)||(SystemTimeToTzSpecificLocalTime(&tzi, &st, &st) == false)) return B_ERRNO;
      }
      v = HumanReadableTimeValues(st.wYear, st.wMonth-1, st.wDay-1, st.wDayOfWeek, st.wHour, st.wMinute, st.wSecond, microsLeft);
      return B_NO_ERROR;
   }
   else return B_ERRNO;
#else
   struct tm ltm, gtm;
   time_t timeS = (time_t) MicrosToSeconds(timeUS);  // timeS is seconds since 1970
   struct tm * ts = (timeType == MUSCLE_TIMEZONE_UTC) ? muscle_localtime_r(&timeS, &ltm) : muscle_gmtime_r(&timeS, &gtm);  // only convert if it isn't already local
   if (ts)
   {
      v = HumanReadableTimeValues(ts->tm_year+1900, ts->tm_mon, ts->tm_mday-1, ts->tm_wday, ts->tm_hour, ts->tm_min, ts->tm_sec, microsLeft);
      return B_NO_ERROR;
   }
   else return B_ERRNO;
#endif
}

#ifdef WIN32
static bool MUSCLE_TzSpecificLocalTimeToSystemTime(LPTIME_ZONE_INFORMATION tzi, LPSYSTEMTIME st)
{
# if defined(__BORLANDC__) || defined(MUSCLE_USING_OLD_MICROSOFT_COMPILER) || defined(__MINGW32__)
#  if defined(_MSC_VER)
   typedef BOOL (*TzSpecificLocalTimeToSystemTimeProc) (IN LPTIME_ZONE_INFORMATION lpTimeZoneInformation, IN LPSYSTEMTIME lpLocalTime, OUT LPSYSTEMTIME lpUniversalTime);
#  elif defined(__MINGW32__) || defined(__MINGW64__)
   typedef BOOL WINAPI (*TzSpecificLocalTimeToSystemTimeProc) (IN LPTIME_ZONE_INFORMATION lpTimeZoneInformation, IN LPSYSTEMTIME lpLocalTime, OUT LPSYSTEMTIME lpUniversalTime);
#  else
   typedef WINBASEAPI BOOL WINAPI (*TzSpecificLocalTimeToSystemTimeProc) (IN LPTIME_ZONE_INFORMATION lpTimeZoneInformation, IN LPSYSTEMTIME lpLocalTime, OUT LPSYSTEMTIME lpUniversalTime);
#  endif

   // Some compilers' headers don't have this call, so we have to do it the hard way
   HMODULE lib = LoadLibrary(TEXT("kernel32.dll"));
   if (lib == NULL) return false;

   TzSpecificLocalTimeToSystemTimeProc tzProc = (TzSpecificLocalTimeToSystemTimeProc) GetProcAddress(lib, "TzSpecificLocalTimeToSystemTime");
   const bool ret = ((tzProc)&&(tzProc(tzi, st, st)));
   FreeLibrary(lib);
   return ret;
# else
   return (TzSpecificLocalTimeToSystemTime(tzi, st, st) != 0);
# endif
}
#endif

status_t GetTimeStampFromHumanReadableTimeValues(const HumanReadableTimeValues & v, uint64 & retTimeStamp, uint32 timeType)
{
   TCHECKPOINT;

#ifdef WIN32
   SYSTEMTIME st; memset(&st, 0, sizeof(st));
   st.wYear         = v.GetYear();
   st.wMonth        = v.GetMonth()+1;
   st.wDayOfWeek    = v.GetDayOfWeek();
   st.wDay          = v.GetDayOfMonth()+1;
   st.wHour         = v.GetHour();
   st.wMinute       = v.GetMinute();
   st.wSecond       = v.GetSecond();
   st.wMilliseconds = v.GetMicrosecond()/1000;

   if (timeType == MUSCLE_TIMEZONE_UTC)
   {
      TIME_ZONE_INFORMATION tzi;
      if ((GetTimeZoneInformation(&tzi) == TIME_ZONE_ID_INVALID)||(MUSCLE_TzSpecificLocalTimeToSystemTime(&tzi, &st) == false)) return B_ERRNO;
   }

   FILETIME fileTime;
   if (SystemTimeToFileTime(&st, &fileTime))
   {
      retTimeStamp = (((((uint64)fileTime.dwHighDateTime)<<32)|((uint64)fileTime.dwLowDateTime))-_windowsDiffTime)/10;
      return B_NO_ERROR;
   }
   else return B_ERRNO;
#else
   struct tm ltm; memset(&ltm, 0, sizeof(ltm));
   ltm.tm_sec  = v.GetSecond();       /* seconds after the minute [0-60] */
   ltm.tm_min  = v.GetMinute();       /* minutes after the hour [0-59] */
   ltm.tm_hour = v.GetHour();         /* hours since midnight [0-23] */
   ltm.tm_mday = v.GetDayOfMonth()+1; /* day of the month [1-31] */
   ltm.tm_mon  = v.GetMonth();        /* months since January [0-11] */
   ltm.tm_year = v.GetYear()-1900;    /* years since 1900 */
   ltm.tm_wday = v.GetDayOfWeek();    /* days since Sunday [0-6] */
   ltm.tm_isdst = -1;  /* Let mktime() decide whether summer time is in effect */

   const time_t tm = ((uint64)((timeType == MUSCLE_TIMEZONE_UTC) ? mktime(&ltm) : timegm(&ltm)));
   if (tm == -1) return B_ERRNO;

   retTimeStamp = SecondsToMicros(tm);
   return B_NO_ERROR;
#endif
}


String HumanReadableTimeValues :: ToString() const
{
   return ExpandTokens("%T");  // Yes, this must be here in the .cpp file!
}

String HumanReadableTimeValues :: ExpandTokens(const String & origString) const
{
   if (origString.IndexOf('%') < 0) return origString;

   String newString = origString;
   (void) newString.Replace("%%", "%");  // do this first!
   (void) newString.Replace("%T", "%Q %D %Y %h:%m:%s");
   (void) newString.Replace("%t", "%Y/%M/%D %h:%m:%s");
   (void) newString.Replace("%f", "%Y-%M-%D_%hh%mm%s");

   static const char * _daysOfWeek[]   = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
   static const char * _monthsOfYear[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

   (void) newString.Replace("%Y", String("%1").Arg(GetYear()));
   (void) newString.Replace("%M", String("%1").Arg(GetMonth()+1,      "%02i"));
   (void) newString.Replace("%Q", String("%1").Arg(_monthsOfYear[muscleClamp(GetMonth(), 0, (int)(ARRAYITEMS(_monthsOfYear)-1))]));
   (void) newString.Replace("%D", String("%1").Arg(GetDayOfMonth()+1, "%02i"));
   (void) newString.Replace("%d", String("%1").Arg(GetDayOfMonth()+1, "%02i"));
   (void) newString.Replace("%W", String("%1").Arg(GetDayOfWeek()+1,  "%02i"));
   (void) newString.Replace("%w", String("%1").Arg(GetDayOfWeek()+1,  "%02i"));
   (void) newString.Replace("%q", String("%1").Arg(_daysOfWeek[muscleClamp(GetDayOfWeek(), 0, (int)(ARRAYITEMS(_daysOfWeek)-1))]));
   (void) newString.Replace("%h", String("%1").Arg(GetHour(),           "%02i"));
   (void) newString.Replace("%m", String("%1").Arg(GetMinute(),         "%02i"));
   (void) newString.Replace("%s", String("%1").Arg(GetSecond(),         "%02i"));
   (void) newString.Replace("%x", String("%1").Arg(GetMicrosecond(),    "%06i"));

   while(newString.Contains("%r"))
   {
      const uint32 r1 = rand();
      const uint32 r2 = rand();
      char buf[64]; muscleSprintf(buf, UINT64_FORMAT_SPEC, (((uint64)r1)<<32)|((uint64)r2));
      if (newString.Replace("%r", buf) <= 0) break;
   }

   return newString;
}

String GetHumanReadableTimeString(uint64 timeUS, uint32 timeType)
{
   TCHECKPOINT;

   if (timeUS == MUSCLE_TIME_NEVER) return ("(never)");
   else
   {
      HumanReadableTimeValues v;
      if (GetHumanReadableTimeValues(timeUS, v, timeType).IsOK())
      {
         char buf[256];
         muscleSprintf(buf, "%02i/%02i/%02i %02i:%02i:%02i", v.GetYear(), v.GetMonth()+1, v.GetDayOfMonth()+1, v.GetHour(), v.GetMinute(), v.GetSecond());
         return String(buf);
      }
      return "";
   }
}

#ifdef WIN32
extern uint64 __Win32FileTimeToMuscleTime(const FILETIME & ft);  // from SetupSystem.cpp
#endif

uint64 ParseHumanReadableTimeString(const String & s, uint32 timeType)
{
   TCHECKPOINT;

   if (s.IndexOfIgnoreCase("never") >= 0) return MUSCLE_TIME_NEVER;

   StringTokenizer tok(s(), "/:");
   const char * year   = tok();
   const char * month  = tok();
   const char * day    = tok();
   const char * hour   = tok();
   const char * minute = tok();
   const char * second = tok();

#if defined(WIN32) && defined(WINXP)
   SYSTEMTIME st; memset(&st, 0, sizeof(st));
   st.wYear      = (WORD) (year   ? atoi(year)   : 0);
   st.wMonth     = (WORD) (month  ? atoi(month)  : 0);
   st.wDay       = (WORD) (day    ? atoi(day)    : 0);
   st.wHour      = (WORD) (hour   ? atoi(hour)   : 0);
   st.wMinute    = (WORD) (minute ? atoi(minute) : 0);
   st.wSecond    = (WORD) (second ? atoi(second) : 0);

   if (timeType == MUSCLE_TIMEZONE_UTC)
   {
      TIME_ZONE_INFORMATION tzi;
      if (GetTimeZoneInformation(&tzi) != TIME_ZONE_ID_INVALID) (void) MUSCLE_TzSpecificLocalTimeToSystemTime(&tzi, &st);
   }

   FILETIME fileTime;
   return (SystemTimeToFileTime(&st, &fileTime)) ? __Win32FileTimeToMuscleTime(fileTime) : 0;
#else
   struct tm st; memset(&st, 0, sizeof(st));
   st.tm_sec  = second ? atoi(second)    : 0;
   st.tm_min  = minute ? atoi(minute)    : 0;
   st.tm_hour = hour   ? atoi(hour)      : 0;
   st.tm_mday = day    ? atoi(day)       : 0;
   st.tm_mon  = month  ? atoi(month)-1   : 0;
   st.tm_year = year   ? atoi(year)-1900 : 0;
   time_t timeS = mktime(&st);
   if (timeType == MUSCLE_TIMEZONE_LOCAL)
   {
      struct tm ltm;
      struct tm * t = muscle_gmtime_r(&timeS, &ltm);
      if (t) timeS += (timeS-mktime(t));
   }
   return SecondsToMicros(timeS);
#endif
}

enum {
   TIME_UNIT_MICROSECOND,
   TIME_UNIT_MILLISECOND,
   TIME_UNIT_SECOND,
   TIME_UNIT_MINUTE,
   TIME_UNIT_HOUR,
   TIME_UNIT_DAY,
   TIME_UNIT_WEEK,
   TIME_UNIT_MONTH,
   TIME_UNIT_YEAR,
   NUM_TIME_UNITS
};

static const uint64 MICROS_PER_DAY = DaysToMicros(1);

static const uint64 _timeUnits[NUM_TIME_UNITS] = {
   1,                       // micros -> micros
   1000,                    // millis -> micros
   MICROS_PER_SECOND,       // secs   -> micros
   60*MICROS_PER_SECOND,    // mins   -> micros
   60*60*MICROS_PER_SECOND, // hours  -> micros
   MICROS_PER_DAY,          // days   -> micros
   7*MICROS_PER_DAY,        // weeks  -> micros
   30*MICROS_PER_DAY,       // months -> micros (well, sort of -- we assume a month is always 30  days, which isn't really true)
   365*MICROS_PER_DAY       // years  -> micros (well, sort of -- we assume a years is always 365 days, which isn't really true)
};
static const char * _timeUnitNames[NUM_TIME_UNITS] = {
   "microsecond",
   "millisecond",
   "second",
   "minute",
   "hour",
   "day",
   "week",
   "month",
   "year",
};

static bool IsFloatingPointNumber(const char * d)
{
   while(1)
   {
           if (*d == '.')            return true;
      else if (isdigit(*d) == false) return false;
      else                           d++;
   }
}

static uint64 GetTimeUnitMultiplier(const String & l, uint64 defaultValue)
{
   uint64 multiplier = defaultValue;
   String tmp(l); tmp = tmp.ToLowerCase();
        if ((tmp.StartsWith("us"))||(tmp.StartsWith("micro"))) multiplier = _timeUnits[TIME_UNIT_MICROSECOND];
   else if ((tmp.StartsWith("ms"))||(tmp.StartsWith("milli"))) multiplier = _timeUnits[TIME_UNIT_MILLISECOND];
   else if (tmp.StartsWith("mo"))                              multiplier = _timeUnits[TIME_UNIT_MONTH];
   else if (tmp.StartsWith("s"))                               multiplier = _timeUnits[TIME_UNIT_SECOND];
   else if (tmp.StartsWith("m"))                               multiplier = _timeUnits[TIME_UNIT_MINUTE];
   else if (tmp.StartsWith("h"))                               multiplier = _timeUnits[TIME_UNIT_HOUR];
   else if (tmp.StartsWith("d"))                               multiplier = _timeUnits[TIME_UNIT_DAY];
   else if (tmp.StartsWith("w"))                               multiplier = _timeUnits[TIME_UNIT_WEEK];
   else if (tmp.StartsWith("y"))                               multiplier = _timeUnits[TIME_UNIT_YEAR];
   return multiplier;
}

uint64 ParseHumanReadableTimeIntervalString(const String & s)
{
   if ((s.EqualsIgnoreCase("forever"))||(s.EqualsIgnoreCase("never"))||(s.StartsWithIgnoreCase("inf"))) return MUSCLE_TIME_NEVER;

   /** Find the first digit in the string */
   const char * digits = s();
   while((*digits)&&(isdigit(*digits) == false)) digits++;
   if (*digits == '\0') return GetTimeUnitMultiplier(s, 0);  // in case the string is just "second" or "hour" or etc.

   /** Find first letter after the digits */
   const char * letters = digits;
   while((*letters)&&(isalpha(*letters) == false)) letters++;
   if (*letters == '\0') letters = "s";  // default to seconds

   const uint64 multiplier = GetTimeUnitMultiplier(letters, _timeUnits[TIME_UNIT_SECOND]);   // default units is seconds

   const char * afterLetters = letters;
   while((*afterLetters)&&((*afterLetters==',')||(isalpha(*afterLetters)||(isspace(*afterLetters))))) afterLetters++;

   uint64 ret = IsFloatingPointNumber(digits) ? (uint64)(atof(digits)*multiplier) : (Atoull(digits)*multiplier);
   if (*afterLetters) ret += ParseHumanReadableTimeIntervalString(afterLetters);
   return ret;
}

static const int64 _largestSigned64BitValue = 0x7FFFFFFFFFFFFFFFLL;  // closest we can get to MUSCLE_TIME_NEVER

int64 ParseHumanReadableSignedTimeIntervalString(const String & s)
{
   const bool isNegative = s.StartsWith('-');
   const uint64 unsignedVal = ParseHumanReadableTimeIntervalString(isNegative ? s.Substring(1) : s);
   return (unsignedVal == MUSCLE_TIME_NEVER) ? _largestSigned64BitValue : (isNegative ? -((int64)unsignedVal) : ((int64)unsignedVal));
}

String GetHumanReadableTimeIntervalString(uint64 intervalUS, uint32 maxClauses, uint64 minPrecision, bool * optRetIsAccurate, bool roundUp)
{
   if (intervalUS == MUSCLE_TIME_NEVER) return "forever";

   // Find the largest unit that is still smaller than (micros)
   uint32 whichUnit = TIME_UNIT_MICROSECOND;
   for (uint32 i=0; i<NUM_TIME_UNITS; i++)
   {
      if (_timeUnits[whichUnit] < intervalUS) whichUnit++;
                                         else break;
   }
   if ((whichUnit >= NUM_TIME_UNITS)||((whichUnit > 0)&&(_timeUnits[whichUnit] > intervalUS))) whichUnit--;

   const uint64 unitSizeUS       = _timeUnits[whichUnit]; 
   const uint64 leftover         = intervalUS%unitSizeUS;
   const bool willAddMoreClauses = ((leftover>minPrecision)&&(maxClauses>1));
   const uint64 numUnits         = (intervalUS/unitSizeUS)+(((roundUp)&&(willAddMoreClauses==false)&&(leftover>=(unitSizeUS/2)))?1:0);
   char buf[256]; muscleSprintf(buf, UINT64_FORMAT_SPEC " %s%s", numUnits, _timeUnitNames[whichUnit], (numUnits==1)?"":"s");
   String ret = buf;

   if (leftover > 0)
   {
      if (willAddMoreClauses) ret += GetHumanReadableTimeIntervalString(leftover, maxClauses-1, minPrecision, optRetIsAccurate).Prepend(", ");
                         else if (optRetIsAccurate) *optRetIsAccurate = false;
   }
   else if (optRetIsAccurate) *optRetIsAccurate = true;

   return ret;
}

String GetHumanReadableSignedTimeIntervalString(int64 intervalUS, uint32 maxClauses, uint64 minPrecision, bool * optRetIsAccurate, bool roundUp)
{
   if (intervalUS == _largestSigned64BitValue) return "forever";  // since we can't use MUSCLE_TIME_NEVER with a signed value, as it comes out as -1

   String ret; 
   if (intervalUS < 0) ret += '-';
   return ret+GetHumanReadableTimeIntervalString(muscleAbs(intervalUS), maxClauses, minPrecision, optRetIsAccurate, roundUp);
}

#ifndef MUSCLE_INLINE_LOGGING

extern uint32 GetAndClearFailedMemoryRequestSize();

void WarnOutOfMemory(const char * file, int line)
{
   // Yes, this technique is open to race conditions and other lossage.
   // But it will work in the one-error-only case, which is good enough
   // for now.
   NestCountGuard ncg(_inWarnOutOfMemory);  // avoid potential infinite recursion if LogCallbacks called by LogTime() try to allocate more memory and also fail
   LogTime(MUSCLE_LOG_CRITICALERROR, "ERROR--MEMORY ALLOCATION FAILURE!  (" INT32_FORMAT_SPEC " bytes at %s:%i)\n", GetAndClearFailedMemoryRequestSize(), file, line);

   if (_inWarnOutOfMemory.IsOutermost())
   {
      static uint64 _prevCallTime = 0;
      if (OnceEvery(SecondsToMicros(5), _prevCallTime)) PrintStackTrace();
   }
}

#endif

} // end namespace muscle
