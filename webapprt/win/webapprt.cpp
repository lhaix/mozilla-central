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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "plstr.h"
#include "prprf.h"
#include "prenv.h"

#include "nsXPCOMGlue.h"
#include "nsINIParser.h"
#include "nsXPCOMPrivate.h"              // for MAXPATHLEN and XPCOM_DLL
#include "nsXULAppAPI.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsILocalFile.h"
#include "nsStringGlue.h"
#include "BinaryPath.h"
#include "nsWindowsWMain.cpp"            // we want a wmain entry point

#include <windows.h>
#include <stdlib.h>
#include <aclapi.h>
#include <shlwapi.h>
#include "shellapi.h"
#define snprintf _snprintf
#define strcasecmp _stricmp

#pragma comment(lib, "version.lib")

XRE_GetFileFromPathType XRE_GetFileFromPath;
XRE_CreateAppDataType XRE_CreateAppData;
XRE_FreeAppDataType XRE_FreeAppData;
XRE_mainType XRE_main;

namespace {
  const char * APP_INI = "application.ini";
  const char * RT_INI = "webapprt.ini";
  const char * APP_RT_BACKUP = "webapprt.old";
  const char * APP_RT = "webapprt.exe";
  const char * REDIT = "redit.exe";
  const char * ICON = "chrome\\icons\\default\\topwindow.ico";

  bool isGreLoaded = false;
  char curEXE[MAXPATHLEN];

  struct Version {
    int major,
          minor,
          revision,
          build;

    bool operator<(Version const& rhs) const {
      return (major < rhs.major
           || minor < rhs.minor
           || revision < rhs.revision
           || build < rhs.build);
    }

    bool operator==(Version const& rhs) const {
      return (major == rhs.major
           && minor == rhs.minor
           && revision == rhs.revision
           && build == rhs.build);
    }
  };

  const Version minVersion = { 13, 0, 0, 0 };

  void Output(const char *fmt, ... ) {
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

    MessageBoxW(NULL, wide_msg, L"WebAppRT", MB_OK);

  #else
    vfprintf(stderr, fmt, ap);
  #endif

    va_end(ap);
  }

  const nsDynamicFunctionLoad kXULFuncs[] = {
      { "XRE_GetFileFromPath", (NSFuncPtr*) &XRE_GetFileFromPath },
      { "XRE_CreateAppData", (NSFuncPtr*) &XRE_CreateAppData },
      { "XRE_FreeAppData", (NSFuncPtr*) &XRE_FreeAppData },
      { "XRE_main", (NSFuncPtr*) &XRE_main },
      { nsnull, nsnull }
  };

  bool AppendLeaf(char *base, const char *toAppend, int baseSize) {
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

  bool StripLeaf(char *base) {
    char *lastSlash = strrchr(base, XPCOM_FILE_PATH_SEPARATOR[0]);
    if (!lastSlash) {
      return false;
    }
    *lastSlash = '\0';
    return true;
  }

  /**
   * Obtains the version number from the specified PE file's version
   * information
   * Version Format: A.B.C.D (Example 10.0.0.300)
   *
   * @param  path The path of the file to check the version on
   * @param  A    The first part of the version number
   * @param  B    The second part of the version number
   * @param  C    The third part of the version number
   * @param  D    The fourth part of the version number
   * @return true if successful
   */
  bool GetVersionFromPath(char *narrowPath, Version &version) {
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
        return false;
    }

    VS_FIXEDFILEINFO *fixedFileInfo = 
      reinterpret_cast<VS_FIXEDFILEINFO *>(fileVersionInfo.get());
    UINT size;
    if (!VerQueryValueW(fileVersionInfo.get(), L"\\", 
      reinterpret_cast<LPVOID*>(&fixedFileInfo), &size)) {
        return false;
    }

    version.major = HIWORD(fixedFileInfo->dwFileVersionMS);
    version.minor = LOWORD(fixedFileInfo->dwFileVersionMS);
    version.revision = HIWORD(fixedFileInfo->dwFileVersionLS);
    version.build = LOWORD(fixedFileInfo->dwFileVersionLS);
    return true;
  }

  nsresult AttemptGRELoad(char *greDir) {
    nsresult rv;

    AppendLeaf(greDir, XPCOM_DLL, MAXPATHLEN);
    rv = XPCOMGlueStartup(greDir);
    StripLeaf(greDir);
    if(NS_FAILED(rv)) {
      return rv;
    }

    rv = XPCOMGlueLoadXULFunctions(kXULFuncs);
    if(NS_FAILED(rv)) {
      return rv;
    }

    SetDllDirectoryA(greDir);

    return rv;
  }

  int CopyAndRun(const char *src, const char *dest) {
    char oldFilePath[MAXPATHLEN];
    strcpy(oldFilePath, dest);
    StripLeaf(oldFilePath);
    AppendLeaf(oldFilePath, APP_RT_BACKUP, MAXPATHLEN);

    if(FALSE == ::MoveFileEx(dest, oldFilePath, MOVEFILE_REPLACE_EXISTING)) {
      return 255;
    }

    if(FALSE == ::CopyFile(src, dest, TRUE)) {
      // Try to move the old file back to its original location
      ::MoveFile(oldFilePath, dest);
      return 255;
    }

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ::ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ::ZeroMemory(&pi, sizeof(pi));

    char reditPath[MAXPATHLEN];
    strcpy(reditPath, src);
    StripLeaf(reditPath);
    AppendLeaf(reditPath, REDIT, MAXPATHLEN);

    char iconPath[MAXPATHLEN];
    strcpy(iconPath, dest);
    StripLeaf(iconPath);
    StripLeaf(iconPath);
    AppendLeaf(iconPath, ICON, MAXPATHLEN);

    char reditArgs[2048];
    sprintf(reditArgs, "\"%s\" \"%s\" \"%s\"", reditPath, dest, iconPath);

    if(!CreateProcess(NULL,           // Module name
                      reditArgs,      // Command line
                      NULL,           // Process handle not inheritable
                      NULL,           // Thread handle not inheritable
                      FALSE,          // Set handle inheritance to FALSE
                      0,              // No creation flags
                      NULL,           // Use parent's environment block
                      NULL,           // Use parent's starting directory 
                      &si,
                      &pi)) {
      // TODO: Maybe log redit failure?
    } else {
      // Wait until child process exits.
      WaitForSingleObject(pi.hProcess, INFINITE);
    }

    ::ZeroMemory(&si, sizeof(si));
    ::ZeroMemory(&pi, sizeof(pi));

    if(!CreateProcess(dest,     // No module name (use command line)
                      NULL,     // Command line
                      NULL,     // Process handle not inheritable
                      NULL,     // Thread handle not inheritable
                      FALSE,    // Set handle inheritance to FALSE
                      0,        // No creation flags
                      NULL,     // Use parent's environment block
                      NULL,     // Use parent's starting directory 
                      &si,
                      &pi)) {
      return GetLastError();
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    if(FALSE != GetExitCodeProcess(pi.hProcess, &exitCode)) {
      exitCode = GetLastError();
    }

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );

    return exitCode;
  }

  bool PerformAppropriateAction(Version const &runningVersion,
                                char *greDir,
                                int *exitCode) {
    bool ret = false;
    Version foundVersion;
    AppendLeaf(greDir, APP_RT, MAXPATHLEN);
    if(FALSE != GetVersionFromPath(greDir, foundVersion)) {
      if(runningVersion == foundVersion) {
        StripLeaf(greDir);
        isGreLoaded = NS_SUCCEEDED(AttemptGRELoad(greDir));
      } else if(minVersion < runningVersion) {
        *exitCode = CopyAndRun(greDir, curEXE);
        ret = true;
      }
    }
    return ret;
  }

  // Copied from toolkit/xre/nsAppData.cpp.
  void SetAllocatedString(const char *&str, const char *newvalue) {
    NS_Free(const_cast<char*>(str));
    if (newvalue) {
      str = NS_strdup(newvalue);
    }
    else {
      str = nsnull;
    }
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
  char appINIPath[MAXPATHLEN];
  char rtINIPath[MAXPATHLEN];
  char greDir[MAXPATHLEN];

  // Get the path and version of our running executable
  if (NS_FAILED(mozilla::BinaryPath::Get(argv[0], curEXE))) {
    Output("Couldn't calculate the application directory.\n");
    return 255;
  }
  Version runningVersion;
  GetVersionFromPath(curEXE, runningVersion);

  // Get the path to application.ini.  This should be in the
  // same directory as the running executable.
  strcpy(appINIPath, curEXE);
  StripLeaf(appINIPath);
  if (!AppendLeaf(appINIPath, APP_INI, MAXPATHLEN)) {
    Output("Path to application is invalid (possibly too long).\n");
    return 255;
  }

  // Open application.ini as an INI file (as opposed to using the
  // XRE application.ini-specific processing we do later)
  nsINIParser parser;
  if(NS_FAILED(parser.Init(appINIPath))) {
    Output("%s was not found\n", appINIPath);
    return 255;
  }

  // Parse WebAppRT options from application.ini
  if(NS_SUCCEEDED(parser.GetString("WebAppRT",
                                   "FirefoxDir",
                                   greDir,
                                   MAXPATHLEN))) {
    int exitCode;
    if(PerformAppropriateAction(runningVersion, greDir, &exitCode)) {
      return exitCode;
    }
  }

  // On Windows, check the registry for a valid FF location
  if(!isGreLoaded) {
    HKEY key;
    // Look for firefox path in additional locations
    RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                 "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App paths\\firefox.exe",
                 0,
                 KEY_READ,
                 &key);
    DWORD length = MAXPATHLEN;
    // XXX: When Vista/XP64 become our minimum supported client, we can use
    //      RegGetValue instead
    RegQueryValueEx(key,
                    "Path",
                    NULL,
                    NULL,
                    reinterpret_cast<BYTE*>(greDir),
                    &length);
    RegCloseKey(key);
    int exitCode;
    if(PerformAppropriateAction(runningVersion, greDir, &exitCode)) {
      return exitCode;
    } else if(isGreLoaded) {
      // TODO: Write gre dir location to application.ini
    }
  }

  if(!isGreLoaded) {
    // TODO: User-friendly message explaining that FF needs to be installed
    return 255;
  }

  // NOTE: The GRE has successfully loaded, so we can use XPCOM now

  { // Scope for any XPCOM stuff we create
    // Set up our environment to know where application.ini was loaded from.
    char appEnv[MAXPATHLEN];
    snprintf(appEnv, MAXPATHLEN, "XUL_APP_FILE=%s", appINIPath);
    if (putenv(appEnv)) {
      Output("Couldn't set %s.\n", appEnv);
      return 255;
    }

    // Get the path to the runtime's INI file.  This should be in the
    // same directory as the GRE.
    strcpy(rtINIPath, greDir);
    if (!AppendLeaf(rtINIPath, RT_INI, MAXPATHLEN)) {
      Output("Path to runtime INI is invalid (possibly too long).\n");
      return 255;
    }

    // Load the runtime's INI from its path.
    nsCOMPtr<nsILocalFile> rtINI;
    if(NS_FAILED(XRE_GetFileFromPath(rtINIPath, getter_AddRefs(rtINI)))) {
      Output("Runtime INI path not recognized: '%s'\n", rtINIPath);
      return 255;
    }
    if(!rtINI) {
      Output("Error: missing webapprt.ini");
      return 255;
    }
    nsXREAppData *webShellAppData;
    if (NS_FAILED(XRE_CreateAppData(rtINI, &webShellAppData))) {
      Output("Couldn't read webapprt.ini\n");
      return 255;
    }

    char profile[MAXPATHLEN];
    if(NS_FAILED(parser.GetString("App",
                                  "Profile",
                                  profile,
                                  MAXPATHLEN))) {
      Output("Unable to retrieve profile from web app INI file");
      return 255;
    }
    SetAllocatedString(webShellAppData->profile, profile);

    nsCOMPtr<nsILocalFile> directory;
    if(NS_FAILED(XRE_GetFileFromPath(greDir,
                                     getter_AddRefs(directory)))) {
      Output("Unable to open app dir");
      return 255;
    }

    nsCOMPtr<nsILocalFile> xreDir;
    if(NS_FAILED(XRE_GetFileFromPath(greDir,
                                     getter_AddRefs(xreDir)))) {
      Output("Unable to open XRE dir");
      return 255;
    }

    xreDir.forget(&webShellAppData->xreDirectory);
    directory.forget(&webShellAppData->directory);

    // There is only XUL.
    result = XRE_main(argc, argv, webShellAppData);

    // Cleanup
    // TODO: The app is about to exit;
    //       do we care about cleaning this stuff up?
    XRE_FreeAppData(webShellAppData);
  }
  XPCOMGlueShutdown();
  return result;
}
