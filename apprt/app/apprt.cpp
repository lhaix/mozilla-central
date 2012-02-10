/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXPCOMGlue.h"
#include "nsINIParser.h"
#include "nsXPCOMPrivate.h" // for MAXPATHLEN and XPCOM_DLL
#include "nsXULAppAPI.h"
#include "nsComponentManagerUtils.h"

#if defined(XP_WIN)
#include <windows.h>
#include <stdlib.h>
#include <aclapi.h>
#include <shlwapi.h>
#include "shellapi.h"
#elif defined(XP_UNIX)
#include <sys/time.h>
#include <sys/resource.h>
#endif

#ifdef XP_MACOSX
#include "MacQuirks.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "plstr.h"
#include "prprf.h"
#include "prenv.h"

#include "nsCOMPtr.h"
#include "nsILocalFile.h"
#include "nsStringGlue.h"

#ifdef XP_WIN
// we want a wmain entry point
#include "nsWindowsWMain.cpp"
#define snprintf _snprintf
#define strcasecmp _stricmp
#endif
#include "BinaryPath.h"


#include "mozilla/Telemetry.h"

#ifdef XP_WIN
#pragma comment(lib, "version.lib")
#endif

#define APP_INI "application.ini"
#define APP_RT "apprt.exe"
#define WEBAPP_SHELL_DIR "webappshell"

XRE_GetFileFromPathType XRE_GetFileFromPath;
XRE_CreateAppDataType XRE_CreateAppData;
XRE_FreeAppDataType XRE_FreeAppData;
#ifdef XRE_HAS_DLL_BLOCKLIST
XRE_SetupDllBlocklistType XRE_SetupDllBlocklist;
#endif
XRE_TelemetryAccumulateType XRE_TelemetryAccumulate;
XRE_mainType XRE_main;

namespace {
  void Output(const char *fmt, ... )
  {
    va_list ap;
    va_start(ap, fmt);

  #if defined(XP_WIN) && !MOZ_WINCONSOLE
    char msg[2048];
    vsnprintf(msg, sizeof(msg), fmt, ap);
    wchar_t wide_msg[1024];
    MultiByteToWideChar(CP_UTF8,
                        0,
                        msg,
                        -1,
                        wide_msg,
                        sizeof(wide_msg) / sizeof(wchar_t));

  MessageBoxW(NULL, wide_msg, L"AppRT", MB_OK);

  #else
    vfprintf(stderr, fmt, ap);
  #endif

    va_end(ap);
  }

  /**
   * Return true if |arg| matches the given argument name.
   */
  bool IsArg(const char* arg, const char* s)
  {
    if (*arg == '-')
    {
      if (*++arg == '-')
        ++arg;
      return !strcasecmp(arg, s);
    }

  #if defined(XP_WIN) || defined(XP_OS2)
    if (*arg == '/')
      return !strcasecmp(++arg, s);
  #endif

    return false;
  }

  const nsDynamicFunctionLoad kXULFuncs[] = {
      { "XRE_GetFileFromPath", (NSFuncPtr*) &XRE_GetFileFromPath },
      { "XRE_CreateAppData", (NSFuncPtr*) &XRE_CreateAppData },
      { "XRE_FreeAppData", (NSFuncPtr*) &XRE_FreeAppData },
  #ifdef XRE_HAS_DLL_BLOCKLIST
      { "XRE_SetupDllBlocklist", (NSFuncPtr*) &XRE_SetupDllBlocklist },
  #endif
      { "XRE_TelemetryAccumulate", (NSFuncPtr*) &XRE_TelemetryAccumulate },
      { "XRE_main", (NSFuncPtr*) &XRE_main },
      { nsnull, nsnull }
  };


  /**
   * Obtains the version number from the specified PE file's version information
   * Version Format: A.B.C.D (Example 10.0.0.300)
   *
   * @param  path The path of the file to check the version on
   * @param  A    The first part of the version number
   * @param  B    The second part of the version number
   * @param  C    The third part of the version number
   * @param  D    The fourth part of the version number
   * @return TRUE if successful
   */
  BOOL GetVersionNumberFromPath(char *narrowPath, DWORD &A, DWORD &B, 
                                DWORD &C, DWORD &D) 
   {
     PRUnichar path[2048];
     ::MultiByteToWideChar(CP_UTF8,
                           nsnull,
                           narrowPath,
                           -1,
                           path,
                           2048);

     DWORD fileVersionInfoSize = GetFileVersionInfoSizeW(path, 0);
     nsAutoArrayPtr<char> fileVersionInfo = new char[fileVersionInfoSize];
     if (!GetFileVersionInfoW(path, 0, fileVersionInfoSize,
                              fileVersionInfo.get())) {
         return FALSE;
     }

     VS_FIXEDFILEINFO *fixedFileInfo = 
       reinterpret_cast<VS_FIXEDFILEINFO *>(fileVersionInfo.get());
     UINT size;
     if (!VerQueryValueW(fileVersionInfo.get(), L"\\", 
       reinterpret_cast<LPVOID*>(&fixedFileInfo), &size)) {
         return FALSE;
     }

     A = HIWORD(fixedFileInfo->dwFileVersionMS);
     B = LOWORD(fixedFileInfo->dwFileVersionMS);
     C = HIWORD(fixedFileInfo->dwFileVersionLS);
     D = LOWORD(fixedFileInfo->dwFileVersionLS);
     return TRUE;
   }

  nsresult AttemptGRELoad(char *xpcomDLL) {
    nsresult rv;
    int gotCounters;
  #if defined(XP_UNIX)
    struct rusage initialRUsage;
    gotCounters = !getrusage(RUSAGE_SELF, &initialRUsage);
  #elif defined(XP_WIN)
    // GetProcessIoCounters().ReadOperationCount seems to have little to
    // do with actual read operations. It reports 0 or 1 at this stage
    // in the program. Luckily 1 coincides with when prefetch is
    // enabled. If Windows prefetch didn't happen we can do our own
    // faster dll preloading.
    IO_COUNTERS ioCounters;
    gotCounters = GetProcessIoCounters(GetCurrentProcess(), &ioCounters);
    if (gotCounters && !ioCounters.ReadOperationCount)
  #endif
    {
        XPCOMGlueEnablePreload();
    }

    rv = XPCOMGlueStartup(xpcomDLL);
    if(NS_FAILED(rv)) {
      return rv;
    }

    rv = XPCOMGlueLoadXULFunctions(kXULFuncs);
    if(NS_FAILED(rv)) {
      return rv;
    }

  #ifdef XRE_HAS_DLL_BLOCKLIST
    XRE_SetupDllBlocklist();
  #endif

    if (gotCounters) {
  #if defined(XP_WIN)
      XRE_TelemetryAccumulate(mozilla::Telemetry::EARLY_GLUESTARTUP_READ_OPS,
                              int(ioCounters.ReadOperationCount));
      XRE_TelemetryAccumulate(
                        mozilla::Telemetry::EARLY_GLUESTARTUP_READ_TRANSFER,
                        int(ioCounters.ReadTransferCount / 1024));
      IO_COUNTERS newIoCounters;
      if (GetProcessIoCounters(GetCurrentProcess(), &newIoCounters)) {
        XRE_TelemetryAccumulate(mozilla::Telemetry::GLUESTARTUP_READ_OPS,
                                int(newIoCounters.ReadOperationCount
                                  - ioCounters.ReadOperationCount));
        XRE_TelemetryAccumulate(mozilla::Telemetry::GLUESTARTUP_READ_TRANSFER,
                                int((newIoCounters.ReadTransferCount
                                   - ioCounters.ReadTransferCount) / 1024));
      }
  #elif defined(XP_UNIX)
      XRE_TelemetryAccumulate(
                        mozilla::Telemetry::EARLY_GLUESTARTUP_HARD_FAULTS,
                        int(initialRUsage.ru_majflt));
      struct rusage newRUsage;
      if (!getrusage(RUSAGE_SELF, &newRUsage)) {
        XRE_TelemetryAccumulate(mozilla::Telemetry::GLUESTARTUP_HARD_FAULTS,
                                int(newRUsage.ru_majflt
                                  - initialRUsage.ru_majflt));
      }
  #endif
    }

    return rv;
  }

  bool AppendFileName(char *base, char *toAppend, int baseSize) {
    if (strlen(base)
      + sizeof(XPCOM_FILE_PATH_SEPARATOR[0])
      + strlen(toAppend)
      + 1
      > baseSize) {
      return false;
    }

    if(base[strlen(base)-1] != XPCOM_FILE_PATH_SEPARATOR[0]) {
        base[strlen(base)+1] = '\0';
        base[strlen(base)] = XPCOM_FILE_PATH_SEPARATOR[0];
    }

    strcpy(base + strlen(base), toAppend);
    return true;
  }

  bool StripFileName(char *base) {
    char *lastSlash = strrchr(base, XPCOM_FILE_PATH_SEPARATOR[0]);
    if (!lastSlash) {
      return false;
    }
    *lastSlash = '\0';
    return true;
  }

  bool VersionsMatch(DWORD a, DWORD b, DWORD c, DWORD d,
                     DWORD e, DWORD f, DWORD g, DWORD h) {
    return(a==e && b==f && c==g && d==h);
  }

  bool CopyAndLaunch(char *src, char *dest) {
    char temp[MAXPATHLEN];
    strcpy(temp, dest);
    StripFileName(temp);
    AppendFileName(temp, "blah.blah", MAXPATHLEN);
    if(FALSE == ::MoveFile(dest, temp)) {
      return false;
    }

    if(FALSE == ::CopyFile(src, dest, TRUE)) {
      return false;
    }

    // "If the function succeeds, it returns a value greater than 32."
    //
    // Source: http://msdn.microsoft.com
    //              /en-us/library/windows
    //              /desktop/bb762153%28v=vs.85%29.aspx
    if(reinterpret_cast<HINSTANCE>(32) >= ::ShellExecute(nsnull,
                                                         nsnull,
                                                         dest,
                                                         nsnull,
                                                         nsnull,
                                                         SW_SHOWDEFAULT)) {
      return false;
    }

    return true;
  }
};


//////////////////////////////////////////////////////////////////////////////
// main
//
// Note: XPCOM cannot be used until AttemptGRELoad has returned successfully.
int main(int argc, char* argv[])
{
  int result = 0;
  int realArgc = 1;
  char **realArgv = argv;
  char curEXE[MAXPATHLEN];
  char appINIPath[MAXPATHLEN];
  char greDir[MAXPATHLEN];
  bool isGreLoaded = false;

#ifdef XP_MACOSX
  TriggerQuirks();
#endif

  // Get the path and version of our running executable
  if (NS_FAILED(mozilla::BinaryPath::Get(argv[0], curEXE))) {
    Output("Couldn't calculate the application directory.\n");
    return 255;
  }
  DWORD a,b,c,d;
  GetVersionNumberFromPath(curEXE, a,b,c,d);

  // Get the path to application.ini.  This should be in the
  // same directory as the running executable.
  strcpy(appINIPath, curEXE);
  StripFileName(appINIPath);
  if (!AppendFileName(appINIPath, APP_INI, MAXPATHLEN)) {
    Output("Path to application.ini is invalid (possibly too long)");
    return 255;
  }

  // Open application.ini as an INI file (as opposed to using the
  // XRE application.ini-specific processing we do later)
  nsINIParser parser;
  if(NS_FAILED(parser.Init(appINIPath))) {
    Output("%s not found.\n", APP_INI);
    return 255;
  }

  // Parse AppRT options from application.ini
  if(NS_SUCCEEDED(parser.GetString("AppRT",
                  "FirefoxDir",
                  greDir,
                  MAXPATHLEN))) {

    // We've retrieved the location in which to find our Firefox binaries.
    // Check whether there is a different version of the AppRT stub there.
    AppendFileName(greDir, APP_RT, MAXPATHLEN);
    DWORD e,f,g,h;
    GetVersionNumberFromPath(greDir, e, f, g, h);
    if(VersionsMatch(a,b,c,d,
                     e,f,g,h)) {
      // The AppRT stub we found is the same version as the one we are
      // currently running.  Let's go ahead and load the GRE from the
      // specified location.
      StripFileName(greDir);
      AppendFileName(greDir, XPCOM_DLL, MAXPATHLEN);
      isGreLoaded = NS_SUCCEEDED(AttemptGRELoad(greDir));
    } else {
      // Copy the AppRT stub we found over the running EXE and launch it.
      CopyAndLaunch(greDir, curEXE);
      // TODO: We probably want to wait for the newly-created process to exit,
      //       then return its return code.  That way if any processes are
      //       waiting on us, we will give them an accurate picture of what
      //       happened during our execution.
      return 0;
    }
  }

  StripFileName(greDir);

  if(!isGreLoaded) {
    // TODO: Look for firefox path in additional locations (reg key?)
    Output("Firefox could not be found at %s", greDir);
    return 255;
  }

  // NOTE: The GRE has successfully loaded, so we can use XPCOM in the
  //       following lines.

  // TODO: If firefox path was found somewhere other than the location
  //       specified in application.ini, write the new location

  { // Scope for any XPCOM stuff we create
    // Set up our environment to know where application.ini was loaded from.
    char appEnv[MAXPATHLEN];
    snprintf(appEnv, MAXPATHLEN, "XUL_APP_FILE=%s", appINIPath);
    if (putenv(appEnv)) {
      Output("Couldn't set %s.\n", appEnv);
      return 255;
    }

    // Load application.ini from the path we got earlier.
    nsCOMPtr<nsILocalFile> appINI;
    if(NS_FAILED(XRE_GetFileFromPath(appINIPath, getter_AddRefs(appINI)))) {
      Output("application.ini path not recognized: '%s'\n", appINIPath);
      return 255;
    }
    if(!appINI) {
      Output("Error: missing application.ini");
      return 255;
    }
    nsXREAppData *webShellAppData;
    if (NS_FAILED(XRE_CreateAppData(appINI, &webShellAppData))) {
      Output("Couldn't read application.ini\n");
      return 255;
    }

    AppendFileName(greDir, WEBAPP_SHELL_DIR, MAXPATHLEN);

    nsCOMPtr<nsILocalFile> webAppShellDir;
    if(NS_FAILED(XRE_GetFileFromPath(greDir, getter_AddRefs(webAppShellDir)))) {
      Output("Unable to open webappshell dir");
      return 255;
    }

    nsCOMPtr<nsIFile> webAppShellDirClone;
    webAppShellDir->Clone(getter_AddRefs(webAppShellDirClone));

    nsCOMPtr<nsILocalFile> webAppShellDirCloneLocal =
                                    do_QueryInterface(webAppShellDirClone);

    webAppShellDir.forget(&webShellAppData->xreDirectory);
    webAppShellDirCloneLocal.forget(&webShellAppData->directory);

    // There is only XUL.
    result = XRE_main(argc, argv, webShellAppData);

    // Cleanup
    // TODO: The app is about to exit; do we care about cleaning this stuff up?
    XRE_FreeAppData(webShellAppData);
  }
  XPCOMGlueShutdown();
  return result;
}
