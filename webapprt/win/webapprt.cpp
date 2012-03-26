/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// System headers (alphabetical)
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <windows.h>

// Mozilla headers (alphabetical)
#include "nsILocalFile.h"
#include "nsINIParser.h"
#include "nsWindowsWMain.cpp"            // we want a wmain entry point
#include "nsXPCOMGlue.h"
#include "nsXPCOMPrivate.h"              // for MAXPATHLEN and XPCOM_DLL
#include "nsXULAppAPI.h"

XRE_GetFileFromPathType XRE_GetFileFromPath;
XRE_CreateAppDataType XRE_CreateAppData;
XRE_FreeAppDataType XRE_FreeAppData;
XRE_mainType XRE_main;

namespace {
  const char kAPP_INI[] = "application.ini";
  const char kWEBAPP_INI[] = "webapp.ini";
  const char kWEBAPPRT_INI[] = "webapprt.ini";
  const char kAPP_ENV_PREFIX[] = "XUL_APP_FILE=";
  const char kAPP_RT[] = "webapprt.exe";

  const wchar_t kICON[] = L"chrome\\icons\\default\\topwindow.ico";
  const wchar_t kAPP_RT_BACKUP[] = L"webapprt.old";

  wchar_t curExePath[MAXPATHLEN];
  wchar_t backupFilePath[MAXPATHLEN];
  wchar_t iconPath[MAXPATHLEN];
  char profile[MAXPATHLEN];
  int* pargc;
  char*** pargv;

  #pragma pack(push, 2)
  typedef struct
  {
    WORD Reserved;
    WORD ResourceType;
    WORD ImageCount;
  } IconHeader;

  typedef struct
  {
    BYTE Width;
    BYTE Height;
    BYTE Colors;
    BYTE Reserved;
    WORD Planes;
    WORD BitsPerPixel;
    DWORD ImageSize;
    DWORD ImageOffset;
  } IconDirEntry;

  typedef struct
  {
    BYTE Width;
    BYTE Height;
    BYTE Colors;
    BYTE Reserved;
    WORD Planes;
    WORD BitsPerPixel;
    DWORD ImageSize;
    WORD ResourceID;    // This field is the one difference to above
  } IconResEntry;
  #pragma pack(pop)

  // Copied from toolkit/xre/nsAppData.cpp.
  void
  SetAllocatedString(const char *&str, const char *newvalue) {
    NS_Free(const_cast<char*>(str));
    if (newvalue) {
      str = NS_strdup(newvalue);
    }
    else {
      str = nsnull;
    }
  }

  nsresult
  joinPath(char* const dest,
           char const* const dir,
           char const* const leaf,
           size_t bufferSize) {
    size_t dirLen = strlen(dir);
    size_t leafLen = strlen(leaf);
    bool needsSeparator = (dirLen != 0
                        && dir[dirLen-1] != '\\'
                        && leafLen != 0
                        && leaf[0] != '\\');

    if(dirLen + (needsSeparator? 1 : 0) + leafLen >= bufferSize) {
      return NS_ERROR_FAILURE;
    }

    strncpy(dest, dir, bufferSize);
    char* destEnd = dest + dirLen;
    if(needsSeparator) {
      *(destEnd++) = '\\';
    }

    strncpy(destEnd, leaf, leafLen);
    return NS_OK;
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

  /**
   * A helper class which calls XPCOMGlueStartup/XPCOMGlueShutdown in its scope.
   */
  class ScopedXPCOMGlue
  {
    public:
      ScopedXPCOMGlue()
        : mRv(NS_ERROR_FAILURE) { }
      ~ScopedXPCOMGlue() {
        if(NS_SUCCEEDED(mRv)) {
          XPCOMGlueShutdown();
        }
      }
      nsresult startup(const char * xpcomFile) {
        return (mRv = XPCOMGlueStartup(xpcomFile));
      }
    private:
      nsresult mRv;
  };

  /**
   * A helper class for scope-guarding a file handle.
   */
  class ScopedFile
  {
    public:
      int* const ptr() { return &mFileHandle; }
      ScopedFile()
        : mFileHandle(-1) { }
      ~ScopedFile() {
        if(-1 != mFileHandle) {
          _close(mFileHandle);
        }
      }

      operator
      int() {
        return get();
      }
    private:
      int mFileHandle;
      int const get() { return mFileHandle; }
  };

  /**
   * A helper class for scope-guarding resource update handles.
   */
  class ScopedResourceUpdateHandle
  {
    public:
      ScopedResourceUpdateHandle()
        : mUpdateRes(NULL) { }

      void
      beginUpdateResource(wchar_t const * const filePath,
                          bool bDeleteExistingResources) {
        mUpdateRes = BeginUpdateResourceW(filePath,
                                          bDeleteExistingResources);
      }

      bool
      commitChanges() {
        bool ret = (FALSE != EndUpdateResourceW(mUpdateRes, FALSE));
        mUpdateRes = NULL;
        return ret;
      }

      ~ScopedResourceUpdateHandle() {
        if(NULL != mUpdateRes) {
          EndUpdateResourceW(mUpdateRes, TRUE);  // Discard changes
        }
      }

      operator
      HANDLE() {
        return get();
      }
    private:
      HANDLE mUpdateRes;
      HANDLE const get() { return mUpdateRes; }
  };

  /**
   * A helper class for scope-guarding nsXREAppData.
   */
  class ScopedXREAppData {
    public:
      ScopedXREAppData()
        : mAppData(NULL) { }

      nsresult
      create(nsILocalFile* aINIFile) {
        return XRE_CreateAppData(aINIFile, &mAppData);
      }

      ~ScopedXREAppData() {
        if(NULL != mAppData) {
          XRE_FreeAppData(mAppData);
        }
      }

      nsXREAppData* const
      operator->() {
        return get();
      }

      nsXREAppData
      operator*() {
        return *get();
      }

      operator
      nsXREAppData*() {
        return get();
      }
    private:
      nsXREAppData* mAppData;
      nsXREAppData* const get() { return mAppData; }
  };

  nsresult
  embedIcon(wchar_t const * const src,
            wchar_t const * const dst) {
    ScopedResourceUpdateHandle updateRes;

    { // Scope for group
      nsAutoArrayPtr<BYTE> group;
      long groupSize;

      { // Scope for data
        nsAutoArrayPtr<BYTE> data;

        { // Scope for file
          ScopedFile file;

          _wsopen_s( file.ptr(),
                     src,
                     _O_BINARY | _O_RDONLY,
                     _SH_DENYWR,
                     _S_IREAD);
          if (file == -1) {
            return NS_ERROR_FAILURE;
          }

          // Load all the data from the icon file
          long filesize = _filelength(file);
          data = new BYTE[filesize];
          _read(file, data, filesize);
        }

        IconHeader* header = reinterpret_cast<IconHeader*>(data.get());

        // Open the target library for updating
        updateRes.beginUpdateResource(dst, FALSE);
        if (updateRes == NULL) {
          return NS_ERROR_FAILURE;
        }

        // Allocate the group resource entry
        groupSize = sizeof(IconHeader) + header->ImageCount * sizeof(IconResEntry);
        group = new BYTE[groupSize];
        memcpy(group, data, sizeof(IconHeader));

        IconDirEntry* sourceIcon = reinterpret_cast<IconDirEntry*>(data + sizeof(IconHeader));
        IconResEntry* targetIcon = reinterpret_cast<IconResEntry*>(group + sizeof(IconHeader));

        for (int id = 1; id <= header->ImageCount; id++) {
          // Add the individual icon
          if (!UpdateResource(updateRes, RT_ICON, MAKEINTRESOURCE(id),
                              MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                              data + sourceIcon->ImageOffset, sourceIcon->ImageSize)) {
            return NS_ERROR_FAILURE;
          }
          // Copy the data for this icon (note that the structs have different sizes)
          memcpy(targetIcon, sourceIcon, sizeof(IconResEntry));
          targetIcon->ResourceID = id;
          sourceIcon++;
          targetIcon++;
        }
      }

      if (!UpdateResource(updateRes, RT_GROUP_ICON, "MAINICON",
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                          group, groupSize)) {
        return NS_ERROR_FAILURE;
      }
    }

    // Save the modifications
    if(!updateRes.commitChanges()) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  void
  Output(const wchar_t *fmt, ... ) {
    va_list ap;
    va_start(ap, fmt);

    wchar_t msg[1024];
    _vsnwprintf_s(msg, _countof(msg), _countof(msg), fmt, ap);

    MessageBoxW(NULL, msg, L"WebappRT", MB_OK);

    va_end(ap);
  }

  void
  Output(const char *fmt, ... ) {
    va_list ap;
    va_start(ap, fmt);

    char msg[1024];
    vsnprintf(msg, sizeof(msg), fmt, ap);

    wchar_t wide_msg[1024];
    MultiByteToWideChar(CP_UTF8,
                        0,
                        msg,
                        -1,
                        wide_msg,
                        _countof(wide_msg));
    Output(wide_msg);

    va_end(ap);
  }

  const nsDynamicFunctionLoad kXULFuncs[] = {
      { "XRE_GetFileFromPath", (NSFuncPtr*) &XRE_GetFileFromPath },
      { "XRE_CreateAppData", (NSFuncPtr*) &XRE_CreateAppData },
      { "XRE_FreeAppData", (NSFuncPtr*) &XRE_FreeAppData },
      { "XRE_main", (NSFuncPtr*) &XRE_main },
      { nsnull, nsnull }
  };

  nsresult
  AttemptCopyAndLaunch(wchar_t* src,
                       int* result) {
    // Rename the old app executable
    if(FALSE == ::MoveFileExW(curExePath,
                              backupFilePath,
                              MOVEFILE_REPLACE_EXISTING)) {
      return NS_ERROR_FAILURE;
    }

    // Copy webapprt.exe from the Firefox dir to the app's dir
    if(FALSE == ::CopyFileW(src,
                            curExePath,
                            TRUE)) {
      // Try to move the old file back to its original location
      ::MoveFileW(backupFilePath,
                  curExePath);
      return NS_ERROR_FAILURE;
    }

    // Embed the app's icon in the new exe
    embedIcon(iconPath,
              curExePath);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ::ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ::ZeroMemory(&pi, sizeof(pi));

    if(!CreateProcessW(curExePath, // No module name (use command line)
                       NULL,             // Command line
                       NULL,             // Process handle not inheritable
                       NULL,             // Thread handle not inheritable
                       FALSE,            // Set handle inheritance to FALSE
                       0,                // No creation flags
                       NULL,             // Use parent's environment block
                       NULL,             // Use parent's starting directory 
                       &si,
                       &pi)) {
      return NS_ERROR_FAILURE;
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    if(FALSE == GetExitCodeProcess(pi.hProcess, &exitCode)) {
      exitCode = GetLastError();
    }

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );

    return NS_OK;
  }

  nsresult
  AttemptCopyAndLaunch(char* srcUtf8, int* result) {
    wchar_t src[MAXPATHLEN];
    if(0 == MultiByteToWideChar(CP_UTF8,
                                0,
                                srcUtf8,
                                -1,
                                src,
                                MAXPATHLEN)) {
      return NS_ERROR_FAILURE;
    }

    return AttemptCopyAndLaunch(src, result);
  }

  nsresult
  AttemptGRELoadAndLaunch(char* greDir,
                          int* result) {
    nsresult rv;

    char xpcomDllPath[MAXPATHLEN];
    rv = joinPath(xpcomDllPath, greDir, XPCOM_DLL, MAXPATHLEN);
    NS_ENSURE_SUCCESS(rv, rv);

    ScopedXPCOMGlue glue;
    rv = glue.startup(xpcomDllPath);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = XPCOMGlueLoadXULFunctions(kXULFuncs);
    NS_ENSURE_SUCCESS(rv, rv);

    // NOTE: The GRE has successfully loaded, so we can use XPCOM now

    wchar_t wideGreDir[MAXPATHLEN];
    if(0 == MultiByteToWideChar(CP_UTF8,
                                0,
                                greDir,
                                -1,
                                wideGreDir,
                                MAXPATHLEN)) {
      return NS_ERROR_FAILURE;
    }

    SetDllDirectoryW(wideGreDir);

    { // Scope for any XPCOM stuff we create

      ScopedLogging log;

      // Get the path to the runtime's INI file.  This should be in the
      // same directory as the GRE.
      char rtIniPath[MAXPATHLEN];
      rv = joinPath(rtIniPath, greDir, kWEBAPPRT_INI, MAXPATHLEN);
      NS_ENSURE_SUCCESS(rv, rv);

      // Load the runtime's INI from its path.
      nsCOMPtr<nsILocalFile> rtINI;
      rv = XRE_GetFileFromPath(rtIniPath, getter_AddRefs(rtINI));
      NS_ENSURE_SUCCESS(rv, rv);

      if(!rtINI) {
        return NS_ERROR_FILE_NOT_FOUND;
      }

      ScopedXREAppData webShellAppData;
      rv = webShellAppData.create(rtINI);
      NS_ENSURE_SUCCESS(rv, rv);

      SetAllocatedString(webShellAppData->profile, profile);

      nsCOMPtr<nsILocalFile> directory;
      rv = XRE_GetFileFromPath(greDir,
                               getter_AddRefs(directory));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsILocalFile> xreDir;
      rv = XRE_GetFileFromPath(greDir,
                               getter_AddRefs(xreDir));
      NS_ENSURE_SUCCESS(rv, rv);

      xreDir.forget(&webShellAppData->xreDirectory);
      directory.forget(&webShellAppData->directory);

      // There is only XUL.
      *result = XRE_main(*pargc, *pargv, webShellAppData);
    }

    return NS_OK;
  }

  nsresult
  AttemptLoadFromDir(char* firefoxDir,
                     int* result) {
    nsresult rv;

    // Here we're going to open Firefox's application.ini
    char appIniPath[MAXPATHLEN];
    rv = joinPath(appIniPath, firefoxDir, kAPP_INI, MAXPATHLEN);
    NS_ENSURE_SUCCESS(rv, rv);

    nsINIParser parser;
    rv = parser.Init(appIniPath);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get buildid of FF we're trying to load
    char buildid[MAXPATHLEN]; // This isn't a path, so MAXPATHLEN doesn't
                              // necessarily make sense, but it's a
                              // convenient number to use.
    rv = parser.GetString("App",
                          "BuildID",
                          buildid,
                          MAXPATHLEN);
    NS_ENSURE_SUCCESS(rv, rv);

    if(0 == strcmp(buildid, NS_STRINGIFY(GRE_BUILDID))) {
      return AttemptGRELoadAndLaunch(firefoxDir, result);
    }

    char webAppRTExe[MAXPATHLEN];
    rv = joinPath(webAppRTExe, firefoxDir, kAPP_RT, MAXPATHLEN);
    NS_ENSURE_SUCCESS(rv, rv);

    return AttemptCopyAndLaunch(webAppRTExe, result);
  }

  nsresult
  GetFirefoxDirFromRegistry(char* firefoxDir) {
    HKEY key;
    wchar_t wideGreDir[MAXPATHLEN];

    if(ERROR_SUCCESS !=
        RegOpenKeyExW(
         HKEY_LOCAL_MACHINE,
         L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App paths\\firefox.exe",
         0,
         KEY_READ,
         &key)) {
      return NS_ERROR_FAILURE;
    }

    DWORD length = MAXPATHLEN * sizeof(wchar_t);
    // XXX: When Vista/XP64 become our minimum supported client, we can use
    //      RegGetValue instead
    if(ERROR_SUCCESS != RegQueryValueExW(key,
                                         L"Path",
                                         NULL,
                                         NULL,
                                         reinterpret_cast<BYTE*>(wideGreDir),
                                         &length)) {
      RegCloseKey(key);
      return NS_ERROR_FAILURE;
    };
    RegCloseKey(key);

    // According to this article, we need to write our own null terminator:
    // http://msdn.microsoft.com/en-us/library/ms724911%28v=vs.85%29.aspx
    length = length / sizeof(wchar_t);
    if(wideGreDir[length] != L'\0') {
      if(length >= MAXPATHLEN) {
        return NS_ERROR_FAILURE;
      }
      wideGreDir[length] = L'\0';
    }

    if(0 == WideCharToMultiByte(CP_UTF8,
                                0,
                                wideGreDir,
                                -1,
                                firefoxDir,
                                MAXPATHLEN,
                                NULL,
                                NULL)) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }
};



//////////////////////////////////////////////////////////////////////////////
// main
//
// Note: XPCOM cannot be used until AttemptGRELoad has returned successfully.
int
main(int argc, char* argv[])
{
  pargc = &argc;
  pargv = &argv;
  int result = 0;
  nsresult rv;
  char buffer[MAXPATHLEN];
  wchar_t wbuffer[MAXPATHLEN];

  // Set up curEXEPath
  if (!GetModuleFileNameW(0, wbuffer, MAXPATHLEN)) {
    Output("Couldn't calculate the application directory.");
    return 255;
  }
  wcsncpy(curExePath, wbuffer, MAXPATHLEN);

  // Get the current directory into wbuffer
  wchar_t* lastSlash = wcsrchr(wbuffer, L'\\');
  if(!lastSlash) {
    Output("Application directory format not understood.");
    return 255;
  }
  *(++lastSlash) = L'\0';

  // Set up icon path
  if(wcslen(wbuffer) + _countof(kICON) >= MAXPATHLEN) {
    Output("Application directory path is too long (couldn't set up icon path).");
  }
  wcsncpy(lastSlash, kICON, _countof(kICON));
  wcsncpy(iconPath, wbuffer, MAXPATHLEN);

  *lastSlash = L'\0';

  // Set up backup file path
  if(wcslen(wbuffer) + _countof(kAPP_RT_BACKUP) >= MAXPATHLEN) {
    Output("Application directory path is too long (couldn't set up backup file path).");
  }
  wcsncpy(lastSlash, kAPP_RT_BACKUP, _countof(kAPP_RT_BACKUP));
  wcsncpy(backupFilePath, wbuffer, MAXPATHLEN);

  *lastSlash = L'\0';

  // Convert current directory to utf8 and stuff it in buffer
  if(0 == WideCharToMultiByte(CP_UTF8,
                              0,
                              wbuffer,
                              -1,
                              buffer,
                              MAXPATHLEN,
                              NULL,
                              NULL)) {
    Output("Application directory could not be processed.");
    return 255;
  }

  // Set up appIniPath with path to webapp.ini.
  // This should be in the same directory as the running executable.
  char appIniPath[MAXPATHLEN];
  if(NS_FAILED(joinPath(appIniPath, buffer, kWEBAPP_INI, MAXPATHLEN))) {
    Output("Path to webapp.ini could not be processed.");
    return 255;
  }

  // Open webapp.ini as an INI file (as opposed to using the
  // XRE webapp.ini-specific processing we do later)
  nsINIParser parser;
  if(NS_FAILED(parser.Init(appIniPath))) {
    Output("Could not open webapp.ini");
    return 255;
  }

  // Set up our environment to know where webapp.ini was loaded from.
  char appEnv[MAXPATHLEN + _countof(kAPP_ENV_PREFIX)];
  strcpy(appEnv, kAPP_ENV_PREFIX);
  strcpy(appEnv + _countof(kAPP_ENV_PREFIX) - 1, appIniPath);
  if (putenv(appEnv)) {
    Output("Couldn't set up app environment");
    return 255;
  }

  // Get profile dir from webapp.ini
  if(NS_FAILED(parser.GetString("Webapp",
                                "Profile",
                                profile,
                                MAXPATHLEN))) {
    Output("Unable to retrieve profile from web app INI file");
    return 255;
  }

  char firefoxDir[MAXPATHLEN];

  { // Scope for first attempt at loading Firefox binaries

    // Get the location of Firefox from our webapp.ini
    // XXX: This string better be UTF-8...
    rv = parser.GetString("WebappRT",
                          "InstallDir",
                          firefoxDir,
                          MAXPATHLEN);
    if(NS_SUCCEEDED(rv)) {
      rv = AttemptLoadFromDir(firefoxDir, &result);
      if(NS_SUCCEEDED(rv)) {
        return result;
      }
    }
  }

  { // Scope for second attempt at loading Firefox binaries

    rv = GetFirefoxDirFromRegistry(firefoxDir);
    if(NS_SUCCEEDED(rv)) {
      rv = AttemptLoadFromDir(firefoxDir, &result);
      if(NS_SUCCEEDED(rv)) {
        // XXX: Write gre dir location to webapp.ini
        return result;
      }
    }
  }

  // We've done all we know how to do to try to find and launch FF
  if(NS_FAILED(rv)) {
    Output("This app requires that Firefox version 14 or above is installed.  Firefox 14+ has not been detected.");
    return 255;
  }
}
