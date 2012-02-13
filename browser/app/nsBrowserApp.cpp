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

#include "application.ini.h"
#include "nsXPCOMGlue.h"
#if defined(XP_WIN)
#include <windows.h>
#include <stdlib.h>
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

#include "nsXPCOMPrivate.h" // for MAXPATHLEN and XPCOM_DLL

#include "mozilla/Telemetry.h"
#include "nsComponentManagerUtils.h"

static void Output(const char *fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

#if defined(XP_WIN) && !MOZ_WINCONSOLE
  PRUnichar msg[2048];
  _vsnwprintf(msg, sizeof(msg)/sizeof(msg[0]), NS_ConvertUTF8toUTF16(fmt).get(), ap);
  MessageBoxW(NULL, msg, L"XULRunner", MB_OK | MB_ICONERROR);
#else
  vfprintf(stderr, fmt, ap);
#endif

  va_end(ap);
}

/**
 * Return true if |arg| matches the given argument name.
 */
static bool IsArg(const char* arg, const char* s)
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

/**
 * A helper class which calls NS_LogInit/NS_LogTerm in its scope.
 */
class ScopedLogging
{
public:
  ScopedLogging() { NS_LogInit(); }
  ~ScopedLogging() { NS_LogTerm(); }
};

XRE_GetFileFromPathType XRE_GetFileFromPath;
XRE_CreateAppDataType XRE_CreateAppData;
XRE_FreeAppDataType XRE_FreeAppData;
#ifdef XRE_HAS_DLL_BLOCKLIST
XRE_SetupDllBlocklistType XRE_SetupDllBlocklist;
#endif
XRE_TelemetryAccumulateType XRE_TelemetryAccumulate;
XRE_mainType XRE_main;

static const nsDynamicFunctionLoad kXULFuncs[] = {
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


//NOTES FOR do_webapp_main
//NEW, EASIER METHOD:
// ----------------
// Firefox changes:
//  load incoming application.ini
//  scribble directory and xredirectory to our custom singleton XUL app
//  use provided profile path in .ini, and direct launch there.  
//  on xul app start, our custom XUL app reads the origin from the .ini file 
//  that was copied to the profile directory at install tim by the addon
//
// Addon changes:
//  during native app install, we create the .ini file as usual
//  we do not symlink to the firefox binaries (we can control our own menubar now...)
//  we create the profile directory for the app, and symlink the applications.sqlite from their 
//    default directory there
//  we copy the app.ini file to the profile directory too, so the XUL app knows which origin to launch

  // XXX This needs to be assembled during the build
  // to include latest Gecko version
  // nsXREAppData webShellAppData = {
  //     sizeof(nsXREAppData),
  //     NULL, // directory will be assigned below
  //     "Mozilla",
  //     "WebRuntime",
  //     "12.0a1", // should come from build
  //     "20120118130802", // should come from build
  //     "{fc882682-1f34-4d82-9dc7-5f31f14f623e}", // not {ec8030f7-c20a-464f-9b0e-13a3a9e97384} which is Firefox
  //     NULL, // copyright
  //     NS_XRE_ENABLE_EXTENSION_MANAGER | NS_XRE_ENABLE_PROFILE_MIGRATOR, // ??? maybe none of these
  //     NULL, // xreDirectory will be assigned below
  //     "12.0a1", // should come from build
  //     "12.0a1", // should come from build
  //     NULL, // crash reporter URL might be nice
  //     NULL};  //should put allocated ownerProfile here in future


static int do_webapp_main(const char *exePath, int argc, char* argv[])
{
  //this is -very- similar yo what happens when we are launched with -app,
  //  except we use a different XUL package to launch with, by setting the
  //  appData.directory and appData.xreDirectory to our new path

  nsCOMPtr<nsILocalFile> appini;
  nsresult rv;
  int result;
  
  // Allow firefox.exe to launch webapps via -webapp <app.ini>
  // Note that -webapp must be the *first* argument.
  if (argc == 2) {
    Output("Incorrect number of arguments passed to -webapp\n");
    return 255;
  }

  rv = XRE_GetFileFromPath(argv[2], getter_AddRefs(appini));
  if (NS_FAILED(rv)) {
    Output("application.ini path not recognized: '%s'\n", argv[2]);
    return 255;
  }

  char appEnv[MAXPATHLEN];
  snprintf(appEnv, MAXPATHLEN, "XUL_APP_FILE=%s", argv[2]);
  if (putenv(appEnv)) {
    Output("Couldn't set %s.\n", appEnv);
    return 255;
  }

  argv[2] = argv[0];
  argv += 2;
  argc -= 2;

  if (!appini) {
    Output("Error: missing application.ini");
    return 255; 
  }

  nsXREAppData *webShellAppData;
  rv = XRE_CreateAppData(appini, &webShellAppData);
  if (NS_FAILED(rv)) {
    Output("Couldn't read application.ini\n");
    return 255;
  }

  //now fiddle the data structure
  char webappShellPath[MAXPATHLEN];
  snprintf(webappShellPath, MAXPATHLEN, "%s%c%s", exePath, XPCOM_FILE_PATH_SEPARATOR[0], "webappshell");
  nsCOMPtr<nsILocalFile> webAppShellDir;

  // exePath comes from mozilla::BinaryPath::Get, which returns a UTF-8
  // encoded path, so it is safe to convert it
#ifdef XP_WIN
  rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(webappShellPath), PR_FALSE,
                       getter_AddRefs(webAppShellDir));
#else
  rv = NS_NewNativeLocalFile(nsDependentCString(webappShellPath), PR_FALSE,
                             getter_AddRefs(webAppShellDir));
#endif
  NS_ENSURE_SUCCESS(rv, rv);
  
  printf("webbappShellPath is %s\n", webappShellPath);
  webShellAppData->directory = webAppShellDir;
  webShellAppData->xreDirectory = webAppShellDir;

  result = XRE_main(argc, argv, webShellAppData);
  XRE_FreeAppData(webShellAppData);

  return result;
}




static int do_main(const char *exePath, int argc, char* argv[])
{
  nsCOMPtr<nsILocalFile> appini;
  nsresult rv;
  int result;
  
  // Allow firefox.exe to launch XULRunner apps via -app <application.ini>
  // Note that -app must be the *first* argument.
  const char *appDataFile = getenv("XUL_APP_FILE");
  if (appDataFile && *appDataFile) {
    rv = XRE_GetFileFromPath(appDataFile, getter_AddRefs(appini));
    if (NS_FAILED(rv)) {
      Output("Invalid path found: '%s'", appDataFile);
      return 255;
    }
  }
  else if (argc > 1 && IsArg(argv[1], "app")) {
    if (argc == 2) {
      Output("Incorrect number of arguments passed to -app");
      return 255;
    }

    rv = XRE_GetFileFromPath(argv[2], getter_AddRefs(appini));
    if (NS_FAILED(rv)) {
      Output("application.ini path not recognized: '%s'", argv[2]);
      return 255;
    }

    char appEnv[MAXPATHLEN];
    snprintf(appEnv, MAXPATHLEN, "XUL_APP_FILE=%s", argv[2]);
    if (putenv(appEnv)) {
      Output("Couldn't set %s.\n", appEnv);
      return 255;
    }
    argv[2] = argv[0];
    argv += 2;
    argc -= 2;
  } else if (argc > 1 && IsArg(argv[1], "webapp")) {
    return do_webapp_main(exePath, argc, argv);
  }

  if (appini) {
    nsXREAppData *appData;
    rv = XRE_CreateAppData(appini, &appData);
    if (NS_FAILED(rv)) {
      Output("Couldn't read application.ini");
      return 255;
    }
    result = XRE_main(argc, argv, appData);
    XRE_FreeAppData(appData);
  } else {
#ifdef XP_WIN
    // exePath comes from mozilla::BinaryPath::Get, which returns a UTF-8
    // encoded path, so it is safe to convert it
    rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(exePath), PR_FALSE,
                         getter_AddRefs(appini));
#else
    rv = NS_NewNativeLocalFile(nsDependentCString(exePath), PR_FALSE,
                               getter_AddRefs(appini));
#endif
    if (NS_FAILED(rv)) {
      return 255;
    }
    result = XRE_main(argc, argv, &sAppData);
  }

  return result;
}

int main(int argc, char* argv[])
{
  char exePath[MAXPATHLEN];

#ifdef XP_MACOSX
  TriggerQuirks();
#endif

  nsresult rv = mozilla::BinaryPath::Get(argv[0], exePath);
  if (NS_FAILED(rv)) {
    Output("Couldn't calculate the application directory.\n");
    return 255;
  }

  char *lastSlash = strrchr(exePath, XPCOM_FILE_PATH_SEPARATOR[0]);
  if (!lastSlash || (lastSlash - exePath > MAXPATHLEN - sizeof(XPCOM_DLL) - 1))
    return 255;

  strcpy(++lastSlash, XPCOM_DLL);

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


  rv = XPCOMGlueStartup(exePath);
  if (NS_FAILED(rv)) {
    Output("Couldn't load XPCOM.\n");
    return 255;
  }
  // Reset exePath so that it is the directory name and not the xpcom dll name
  *lastSlash = 0;

  rv = XPCOMGlueLoadXULFunctions(kXULFuncs);
  if (NS_FAILED(rv)) {
    Output("Couldn't load XRE functions.\n");
    return 255;
  }

#ifdef XRE_HAS_DLL_BLOCKLIST
  XRE_SetupDllBlocklist();
#endif

  if (gotCounters) {
#if defined(XP_WIN)
    XRE_TelemetryAccumulate(mozilla::Telemetry::EARLY_GLUESTARTUP_READ_OPS,
                            int(ioCounters.ReadOperationCount));
    XRE_TelemetryAccumulate(mozilla::Telemetry::EARLY_GLUESTARTUP_READ_TRANSFER,
                            int(ioCounters.ReadTransferCount / 1024));
    IO_COUNTERS newIoCounters;
    if (GetProcessIoCounters(GetCurrentProcess(), &newIoCounters)) {
      XRE_TelemetryAccumulate(mozilla::Telemetry::GLUESTARTUP_READ_OPS,
                              int(newIoCounters.ReadOperationCount - ioCounters.ReadOperationCount));
      XRE_TelemetryAccumulate(mozilla::Telemetry::GLUESTARTUP_READ_TRANSFER,
                              int((newIoCounters.ReadTransferCount - ioCounters.ReadTransferCount) / 1024));
    }
#elif defined(XP_UNIX)
    XRE_TelemetryAccumulate(mozilla::Telemetry::EARLY_GLUESTARTUP_HARD_FAULTS,
                            int(initialRUsage.ru_majflt));
    struct rusage newRUsage;
    if (!getrusage(RUSAGE_SELF, &newRUsage)) {
      XRE_TelemetryAccumulate(mozilla::Telemetry::GLUESTARTUP_HARD_FAULTS,
                              int(newRUsage.ru_majflt - initialRUsage.ru_majflt));
    }
#endif
  }

  int result;
  {
    ScopedLogging log;
    result = do_main(exePath, argc, argv);
  }

  XPCOMGlueShutdown();
  return result;
}
