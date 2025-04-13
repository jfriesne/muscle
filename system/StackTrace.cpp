/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#if defined(__EMSCRIPTEN__)
# include <emscripten/emscripten.h>
#endif

#include "util/String.h"

#ifdef MUSCLE_USE_MSVC_STACKWALKER
# include <dbghelp.h>
# if defined(UNICODE) && !defined(_UNICODE)
#  define _UNICODE 1
# endif
# include <tchar.h>
#endif

#if defined(__APPLE__)
# include "AvailabilityMacros.h"  // so we can find out if this version of MacOS/X is new enough to include backtrace() and friends
#endif

#if (defined(__linux__) && !defined(ANDROID)) || (defined(MAC_OS_X_VERSION_10_5) && defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5))
# include <execinfo.h>
# define MUSCLE_USE_BACKTRACE 1
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
 *   http://www.codeproject.com/KB/threads/StackTrace.aspx
 *
 **********************************************************************/

class StackTraceInternal;  // forward
class StackTrace
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

   StackTrace(
     const OutputPrinter & printer,
     int options = OptionsAll, // 'int' is by design, to combine the enum-flags
     LPTSTR szSymPath = NULL,
     DWORD dwProcessId = GetCurrentProcessId(),
     HANDLE hProcess = GetCurrentProcess()
     );
   ~StackTrace();

   typedef BOOL (__stdcall *PReadProcessMemoryRoutine)(
     HANDLE      hProcess,
     DWORD64     qwBaseAddress,
     PVOID       lpBuffer,
     DWORD       nSize,
     LPDWORD     lpNumberOfBytesRead,
     LPVOID      pUserData  // optional data, which was passed in "ShowCallstack"
     );

   BOOL LoadModules();

   status_t ShowCallstack(
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
      m_printer.puts("  ");
#ifdef _UNICODE
      size_t len;
      (void) wcstombs_s(&len, NULL, 0, szText, wcslen(szText));
      char * buf = new char[len];
      (void) wcstombs_s(&len, buf, len, szText, wcslen(szText));
      if (len > 0) m_printer.puts(buf);
      delete [] buf;
#else
      m_printer.puts(szText);
#endif
   }

   StackTraceInternal *m_sw;
   HANDLE m_hProcess;
   DWORD m_dwProcessId;
   BOOL m_modulesLoaded;
   LPTSTR m_szSymPath;

   const OutputPrinter m_printer;

   int m_options;

   static BOOL __stdcall ReadProcMemCallback(HANDLE hProcess, DWORD64 qwBaseAddress, PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead);

   friend class StackTraceInternal;
};

// Called from code in MiscUtilityFunctions.cpp
void _Win32PrintStackTraceForContext(const OutputPrinter & p, CONTEXT * context, uint32 maxDepth)
{
   p.printf("--Stack trace follows:\n");
   (void) StackTrace(p, StackTrace::OptionsJAF).ShowCallstack(maxDepth, GetCurrentThread(), context);
   p.printf("--End Stack trace\n");
}

// Some missing defines (for VC5/6):
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

// Normally it should be enough to use 'CONTEXT_FULL' (better would be 'CONTEXT_ALL')
#define USED_CONTEXT_FLAGS CONTEXT_FULL

class StackTraceInternal
{
public:
  StackTraceInternal(StackTrace *parent, HANDLE hProcess)
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
  ~StackTraceInternal()
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
    if (pSI(m_hProcess, m_szSymPath, FALSE) == FALSE)
      m_parent->OnDbgHelpErr(_T("SymInitialize"), GetLastError(), 0);

    DWORD symOptions = pSGO();  // SymGetOptions
    symOptions |= SYMOPT_LOAD_LINES;
    symOptions |= SYMOPT_FAIL_CRITICAL_ERRORS;
    //symOptions |= SYMOPT_NO_PROMPTS;
    // SymSetOptions
    symOptions = pSSO(symOptions);

    TCHAR buf[StackTrace::STACKWALK_MAX_NAMELEN] = {0};
    if (pSGSP != NULL)
    {
      if (pSGSP(m_hProcess, buf, StackTrace::STACKWALK_MAX_NAMELEN) == FALSE)
        m_parent->OnDbgHelpErr(_T("SymGetSearchPath"), GetLastError(), 0);
    }
    TCHAR szUserName[1024] = {0};
    DWORD dwSize = 1024;
    GetUserName(szUserName, &dwSize);
    m_parent->OnSymInit(buf, symOptions, szUserName);

    return TRUE;
  }

  StackTrace *m_parent;

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
      LoadModule(hProcess, me.szExePath, me.szModule, (DWORD64) me.modBaseAddr, me.modBaseSize);
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

      DWORD dwRes = LoadModule(hProcess, tt, tt2, (DWORD64) mi.lpBaseOfDll, mi.SizeOfImage);
      if (dwRes != ERROR_SUCCESS) m_parent->OnDbgHelpErr(_T("LoadModule"), dwRes, 0);
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
      if ( (m_parent->m_options & StackTrace::RetrieveFileVersion) != 0)
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
      if (GetModuleInfo(hProcess, baseAddr, &Module) != FALSE)
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
      m_parent->OnLoadModule(img, mod, baseAddr, size, result, szSymType, Module.LoadedImageName, fileVersion);
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
    if(pSGMI == NULL)
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
    if (pSGMI(hProcess, baseAddr, (IMAGEHLP_MODULEW64*) pData) != FALSE)
#else
    memcpy(pData, pModuleInfo, sizeof(IMAGEHLP_MODULE64));
    if (pSGMI(hProcess, baseAddr, (IMAGEHLP_MODULE64*) pData) != FALSE)
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
StackTrace::StackTrace(const OutputPrinter & p, int options, LPTSTR szSymPath, DWORD dwProcessId, HANDLE hProcess)
   : m_printer(p)
   , m_options(options)
   , m_modulesLoaded(FALSE)
   , m_hProcess(hProcess)
   , m_sw(new StackTraceInternal(this, m_hProcess))
   , m_dwProcessId(dwProcessId)
{
  if (szSymPath != NULL)
  {
    m_szSymPath = _tcsdup(szSymPath);
    m_options |= SymBuildPath;
  }
  else m_szSymPath = NULL;
}

StackTrace::~StackTrace()
{
  if (m_szSymPath) free(m_szSymPath);
  delete m_sw;
}

BOOL StackTrace::LoadModules()
{
  if (m_sw == NULL)
  {
    SetLastError(ERROR_DLL_INIT_FAILED);
    return FALSE;
  }
  if (m_modulesLoaded != FALSE)
    return TRUE;

  // Build the sym-path:
  TCHAR *szSymPath = NULL;
  if ( (m_options & SymBuildPath) != 0)
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
    if (m_szSymPath != NULL)
    {
      _tcscat_s(szSymPath, nSymPathLen, m_szSymPath);
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

    if ( (m_options & SymBuildPath) != 0)
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
  BOOL bRet = m_sw->Init(szSymPath);
  if (szSymPath != NULL) free(szSymPath); szSymPath = NULL;
  if (bRet == FALSE)
  {
    OnDbgHelpErr(_T("Error while initializing dbghelp.dll"), 0, 0);
    SetLastError(ERROR_DLL_INIT_FAILED);
    return FALSE;
  }

  bRet = m_sw->LoadModules(m_hProcess, m_dwProcessId);
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
static StackTrace::PReadProcessMemoryRoutine s_readMemoryFunction = NULL;
static LPVOID s_readMemoryFunction_UserData = NULL;

status_t StackTrace::ShowCallstack(uint32 maxDepth, HANDLE hThread, const CONTEXT *context, PReadProcessMemoryRoutine readMemoryFunction, LPVOID pUserData)
{
  CallstackEntry *csEntry = NULL;  // deliberately declared here because declaring it later causes MSVC to error out due to gotos skipping the declaration

#ifdef _UNICODE
  SYMBOL_INFOW *pSym = NULL;
  IMAGEHLP_MODULEW64 Module;
  IMAGEHLP_LINEW64 Line;
#else
  IMAGEHLP_SYMBOL64 *pSym = NULL;
  IMAGEHLP_MODULE64 Module;
  IMAGEHLP_LINE64 Line;
#endif

  if (m_modulesLoaded == FALSE) (void) LoadModules();  // yes, deliberately ignoring the result

  if (m_sw->m_hDbhHelp == NULL)
  {
    SetLastError(ERROR_DLL_INIT_FAILED);
    return B_ERROR("DebugHelp DLL not found");
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
      return B_ERROR("GetThreadContext() failed");
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
# error "StackTrace:  Platform not supported!"
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

  csEntry = new CallstackEntry;
  for (uint32 frameNum=0; frameNum<maxDepth; frameNum++)
  {
    // get next stack frame (StackWalk64(), SymFunctionTableAccess64(), SymGetModuleBase64())
    // if this returns ERROR_INVALID_ADDRESS (487) or ERROR_NOACCESS (998), you can
    // assume that either you are done, or that the stack is so hosed that the next
    // deeper frame could not be found.
    // CONTEXT need not to be supplied if imageType is IMAGE_FILE_MACHINE_I386!
    if ( ! m_sw->pSW(imageType, m_hProcess, hThread, &s, &_context, ReadProcMemCallback, m_sw->pSFTA, m_sw->pSGMB, NULL) )
    {
      OnDbgHelpErr(_T("StackWalk64"), GetLastError(), s.AddrPC.Offset);
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
      OnDbgHelpErr(_T("StackWalk64-Endless-Callstack!"), 0, s.AddrPC.Offset);
      break;
    }
    if (s.AddrPC.Offset != 0)
    {
      // we seem to have a valid PC
      // show procedure info (SymGetSymFromAddr64())
#ifdef _UNICODE
      if (m_sw->pSFA(m_hProcess, s.AddrPC.Offset, &(csEntry->offsetFromSymbol), pSym) != FALSE)
#else
      if (m_sw->pSGSFA(m_hProcess, s.AddrPC.Offset, &(csEntry->offsetFromSymbol), pSym) != FALSE)
#endif
      {
        // TODO: Mache dies sicher...!
        _tcscpy_s(csEntry->name, pSym->Name);
        // UnDecorateSymbolName()
        m_sw->pUDSN( pSym->Name, csEntry->undName, STACKWALK_MAX_NAMELEN, UNDNAME_NAME_ONLY );
        m_sw->pUDSN( pSym->Name, csEntry->undFullName, STACKWALK_MAX_NAMELEN, UNDNAME_COMPLETE );
      }
      else
      {
#ifdef _UNICODE
        OnDbgHelpErr(_T("SymFromAddrW"), GetLastError(), s.AddrPC.Offset);
#else
        OnDbgHelpErr(_T("SymGetSymFromAddr64"), GetLastError(), s.AddrPC.Offset);
#endif
      }

      // show line number info, NT5.0-method (SymGetLineFromAddr64())
      if (m_sw->pSGLFA != NULL )
      { // yes, we have SymGetLineFromAddr64()
        if (m_sw->pSGLFA(m_hProcess, s.AddrPC.Offset, &(csEntry->offsetFromLine), &Line) != FALSE)
        {
          csEntry->lineNumber = Line.LineNumber;
          // TODO: Mache dies sicher...!
          _tcscpy_s(csEntry->lineFileName, Line.FileName);
        }
        else
        {
          OnDbgHelpErr(_T("SymGetLineFromAddr64"), GetLastError(), s.AddrPC.Offset);
        }
      } // yes, we have SymGetLineFromAddr64()

      // show module info (SymGetModuleInfo64())
      if (m_sw->GetModuleInfo(m_hProcess, s.AddrPC.Offset, &Module ) != FALSE)
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
      else OnDbgHelpErr(_T("SymGetModuleInfo64"), GetLastError(), s.AddrPC.Offset);
    } // we seem to have a valid PC

    CallstackEntryType et = (frameNum == 0) ? firstEntry : nextEntry;
    OnCallstackEntry(et, csEntry);

    if (s.AddrReturn.Offset == 0)
    {
      OnCallstackEntry(lastEntry, csEntry);
      SetLastError(ERROR_SUCCESS);
      break;
    }
  } // for ( frameNum )

  cleanup:
    if (csEntry) delete csEntry;
    if (pSym) free( pSym );

  if (context == NULL)
    ResumeThread(hThread);

  return B_NO_ERROR;
}

BOOL __stdcall StackTrace::ReadProcMemCallback(
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

void StackTrace::OnLoadModule(LPTSTR img, LPTSTR mod, DWORD64 baseAddr, DWORD size, DWORD result, LPCTSTR symType, LPTSTR pdbName, ULONGLONG fileVersion)
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

void StackTrace::OnCallstackEntry(CallstackEntryType eType, CallstackEntry *entry)
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

void StackTrace::OnDbgHelpErr(LPCTSTR szFuncName, DWORD gle, DWORD64 addr)
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

void StackTrace::OnSymInit(LPTSTR szSearchPath, DWORD symOptions, LPTSTR szUserName)
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

status_t PrintStackTrace(const OutputPrinter & p, uint32 maxDepth)
{
   status_t ret;

#if defined(__EMSCRIPTEN__)
   (void) p;
   (void) maxDepth;
   emscripten_run_script("console.log(new Error().stack)");
#elif defined(MUSCLE_USE_BACKTRACE)
   void *array[256];
   const size_t size = backtrace(array, muscleMin(maxDepth, ARRAYITEMS(array)));

   p.printf("--Stack trace follows (%zd frames):", size);

   FILE * f = p.GetFile();
   const int fd = f ? fileno(f) : -1;
   if (fd >= 0) backtrace_symbols_fd(array, (int)size, fd); // avoids a heap-allocation when possible
   else
   {
      char ** strings = backtrace_symbols(array, (int)size);
      if (strings)
      {
         for (size_t i = 0; i < size; i++)
         {
            p.puts("\n  ");
            p.puts(strings[i]);
         }
         p.puts("\n--End Stack trace\n");
         free(strings);
      }
      else ret = B_OUT_OF_MEMORY;
   }

   p.printf("--End Stack trace\n");
#elif defined(MUSCLE_USE_MSVC_STACKWALKER)
   return StackTrace(p, StackTrace::OptionsJAF).ShowCallstack(maxDepth);
#else
   (void) p;   // shut the compiler up
   (void) maxDepth;
   ret = B_UNIMPLEMENTED;
#endif

   return ret;
}

#ifndef MUSCLE_INLINE_LOGGING

// These functions are deliberately defined here because if I make them inline
// function then OutputPrinter.h has to be #included beforehand, and that
// leads to some compile-time chicken-and-egg problems that I'd rather avoid.
status_t LogStackTrace(int logSeverity, uint32 maxDepth) {return PrintStackTrace(logSeverity, maxDepth);}
status_t PrintStackTrace(               uint32 maxDepth) {return PrintStackTrace(stdout,      maxDepth);}

# ifdef MUSCLE_RECORD_REFCOUNTABLE_ALLOCATION_LOCATIONS
void UpdateAllocationStackTrace(bool isAllocation, String * & s)
{
   if (isAllocation)
   {
      if (s == NULL) s = new String;

      status_t ret;
      if (PrintStackTrace(*s).IsError(ret)) *s = String("(no stack trace available: %s)").Arg(ret());
   }
   else
   {
      delete s;
      s = NULL;
   }
}

void PrintAllocationStackTrace(const OutputPrinter & p, const void * slabThis, const void * obj, uint32 slabIdx, uint32 numObjectsPerSlab, const String & stackStr)
{
   p.printf("\nObjectSlab %p:  Object %p (#" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ") was allocated at this location:\n", slabThis, obj, slabIdx, numObjectsPerSlab);
   p.puts(stackStr());
}
# endif

#endif  // end !MUSCLE_INLINE_LOGGING section

} // end namespace muscle
