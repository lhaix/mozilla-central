/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//PLAN:

 // open my bundle, check for an override binary signature
 // find the newest firefox. open its bundle, get the version number.
 // if the firefox version is different than ours:
 //   delete our own binary, (unlink)
 //   copy the newer binary and bundle file from Firefox
 //   execv it, and quit


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>


#include "nsXPCOMGlue.h"
#include "nsINIParser.h"
#include "nsXPCOMPrivate.h"              // for MAXPATHLEN and XPCOM_DLL
#include "nsXULAppAPI.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsILocalFile.h"
#include "nsStringGlue.h"



const char WEBAPPRT_EXECUTABLE[] = "webapprt";
const char APPINI_NAME[] = "application.ini";
const char WEBRTINI_NAME[] = "webapprt.ini";

//need the correct relative path here
const char WEBAPPRT_PATH[] = "/Contents/MacOS/"; 
const char INFO_FILE_PATH[] = "/Contents/Info.plist";

void execNewBinary(NSString* launchPath);

NSString *pathToWebRT(NSString* alternateBinaryID);

NSException* makeException(NSString* name, NSString* message);

void displayErrorAlert(NSString* title, NSString* message);

//this is our version, to be compared with the version of the binary we are asked to use
NSString* myVersion = [NSString stringWithFormat:@"%s", NS_STRINGIFY(GRE_MILESTONE)];

int gVerbose = 0;

XRE_GetFileFromPathType XRE_GetFileFromPath;
XRE_CreateAppDataType XRE_CreateAppData;
XRE_FreeAppDataType XRE_FreeAppData;
XRE_mainType XRE_main;

  const nsDynamicFunctionLoad kXULFuncs[] = 
  {
      { "XRE_GetFileFromPath", (NSFuncPtr*) &XRE_GetFileFromPath },
      { "XRE_CreateAppData", (NSFuncPtr*) &XRE_CreateAppData },
      { "XRE_FreeAppData", (NSFuncPtr*) &XRE_FreeAppData },
      { "XRE_main", (NSFuncPtr*) &XRE_main },
      { nsnull, nsnull }
  };

  nsresult AttemptGRELoad(char *greDir) 
  {
    nsresult rv;
    char xpcomDLLPath[MAXPATHLEN];
    snprintf(xpcomDLLPath, MAXPATHLEN, "%s%s", greDir, XPCOM_DLL);

    rv = XPCOMGlueStartup(xpcomDLLPath);

    if(NS_FAILED(rv)) 
    {
      return rv;
    }

    rv = XPCOMGlueLoadXULFunctions(kXULFuncs);
    if(NS_FAILED(rv)) 
    {
      return rv;
    }

    return rv;
  }

  // Copied from toolkit/xre/nsAppData.cpp.
  void SetAllocatedString(const char *&str, const char *newvalue) 
  {
    NS_Free(const_cast<char*>(str));
    if (newvalue) 
    {
      str = NS_strdup(newvalue);
    }
    else 
    {
      str = nsnull;
    }
  }


int main(int argc, char **argv)
{
  int i;
  NSString *firefoxPath = nil;   
  NSString *alternateBinaryID = nil;

  for (i=1;i < argc;i++)
  {
    if (!strcmp(argv[i], "-v")) 
    {
      gVerbose = 1;
    } 
    else if (!strcmp(argv[i], "-b")) 
    {
      if (i+1 < argc) 
      {
        alternateBinaryID = [NSString stringWithFormat: @"%s", argv[i+1]];
        i++;
      }
    }
  }

  NSLog(@"MY WEBAPPRT VERSION: %@", myVersion);

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];  

  //I need to look in our bundle first, before deciding what firefox binary to use
  NSString* myBundlePath = [[NSBundle mainBundle] bundlePath];
  NSString* myInfoFilePath = [NSString stringWithFormat:@"%@%s", myBundlePath, INFO_FILE_PATH];
  if (![[NSFileManager defaultManager] fileExistsAtPath:myInfoFilePath]) 
  {
    //get out of here, I don't have a bundle file?
    NSLog(@"webapp bundle file not found at path: %@", myInfoFilePath);
    @throw makeException(@"Invalid App Package", @"This App appears to be damaged, please reinstall it");
  }

  NSDictionary *myAppInfo = [NSDictionary dictionaryWithContentsOfFile:myInfoFilePath];
  if (!myAppInfo) 
  {
    @throw makeException(@"Unable To Read App Version", @"This App appears to be damaged, please reinstall it");
  }

  //see if the Info.plist file specifies a different Firefox to use
  alternateBinaryID = [myAppInfo objectForKey:@"FirefoxBinary"];
  NSLog(@"found override firefox binary: %@", alternateBinaryID);

  @try 
  {

    if (!firefoxPath) 
    {
      firefoxPath = pathToWebRT(alternateBinaryID); // specify an alternate firefox to use in the Info.plist file
      if (!firefoxPath) 
      {
        // Launch a dialog to explain to the user that there's no compatible web runtime
        NSLog(@"unable to find a valid webrt path");
        @throw makeException(@"Missing Web Runtime", @"Web Applications require Firefox to be installed");
      }
    }  


    NSString *myWebRTPath = [NSString stringWithFormat: @"%@%s%s", myBundlePath, WEBAPPRT_PATH, WEBAPPRT_EXECUTABLE];
    NSLog(@"my webrt path: %@", myWebRTPath);
    if (![[NSFileManager defaultManager] fileExistsAtPath:myWebRTPath]) 
    {
      @throw makeException(@"Missing WebRT Files", @"Cannot locate binary for this application");
    }

    //Check to see if the DYLD_FALLBACK_LIBRARY_PATH is set. if not, set it, and relaunch ourselves
    char libEnv[MAXPATHLEN];
    snprintf(libEnv, MAXPATHLEN, "%s%s", [firefoxPath UTF8String], WEBAPPRT_PATH);

    char* curVal = getenv("DYLD_FALLBACK_LIBRARY_PATH");
    //NSLog(@"libenv: %s", libEnv);
    //NSLog(@"curval: %s", curVal);

    if ((curVal == NULL) || strncmp(libEnv, curVal, MAXPATHLEN)) 
    {
      NSLog(@"DYLD_FALLBACK_LIBRARY_PATH NOT SET!!");
      //they differ, so set it and relaunch
      setenv("DYLD_FALLBACK_LIBRARY_PATH", libEnv, 1);
      execNewBinary(myWebRTPath);
      exit(0);
    }
    //NSLog(@"Set DYLD_FALLBACK_LIBRARY_PATH to: %s", libEnv);


    //now get Firefox version, but read-only, which is much simpler.
    NSString* firefoxInfoFile = [NSString stringWithFormat:@"%@%s", firefoxPath, INFO_FILE_PATH];
    if (![[NSFileManager defaultManager] fileExistsAtPath:firefoxInfoFile]) 
    {
      //get out of here, I don't have a bundle file?
      NSLog(@"Firefox bundle file not found at path: %@", firefoxInfoFile);
      @throw makeException(@"Missing Application File", @"This copy of Firefox appears to be damaged (unable to locate Info.plist)");
    }

    //get read-only copy of FF Info.plist file
    NSDictionary *firefoxAppInfo = [NSDictionary dictionaryWithContentsOfFile:firefoxInfoFile];
    if (!firefoxAppInfo) 
    {
      @throw makeException(@"Unreadable Info File", @"This copy of Firefox appears to be damaged (Info.plist Unreadable)");
    }
    NSString* firefoxVersion = [firefoxAppInfo objectForKey:@"CFBundleVersion"];
    //Finished getting Firefox version


    //compare them
    if ([myVersion compare: firefoxVersion] != NSOrderedSame) 
    {
      //we are going to assume that if they are different, we need to re-copy the webapprt, regardless of whether
      // it is newer or older.  If we don't find a webapprt, then the current Firefox must not be new enough to run webapps.
      NSLog(@"This Application has an old webrt. Updating it.");

      //we know the firefox path, so copy the new webapprt here
      NSString *newWebRTPath = [NSString stringWithFormat: @"%@%s%s", firefoxPath, WEBAPPRT_PATH, WEBAPPRT_EXECUTABLE];
      NSLog(@"firefox webrt path: %@", newWebRTPath);
      if (![[NSFileManager defaultManager] fileExistsAtPath:newWebRTPath]) 
      {
        NSString* msg = [NSString stringWithFormat: @"This version of Firefox (%@) cannot run web applications, because it is not recent enough or damaged", firefoxVersion];
        @throw makeException(@"Missing WebRT Files", msg);
      }

          //unlink my binary file
      int err = unlink([myWebRTPath UTF8String]);
      if (err) 
      {
        NSLog(@"failed to unlink old binary file at path: %@", myWebRTPath);
        @throw makeException(@"Unable To Update", @"Failed preparation for runtime update");
      }

      NSError *errorDesc = nil;
      NSFileManager* clerk = [[NSFileManager alloc] init];
      [clerk copyItemAtPath: newWebRTPath toPath: myWebRTPath error: &errorDesc];
      [clerk release];
      if (errorDesc != nil) 
      {
        NSLog(@"failed to copy new webrt file");
        @throw makeException(@"Unable To Update", @"Failed to update runtime");
      }

    //execv the new binary, and ride off into the sunset
    execNewBinary(myWebRTPath);

  } 
  else 
  {
    //we are ready to load XUL and such, and go go go

      NSLog(@"This Application has the newest webrt.  Launching!");
      bool isGreLoaded = false;

      int result = 0;
      char rtINIPath[MAXPATHLEN];

      // Set up our environment to know where application.ini was loaded from.
      char appEnv[MAXPATHLEN];
      snprintf(appEnv, MAXPATHLEN, "%s%s%s", [myBundlePath UTF8String], WEBAPPRT_PATH, APPINI_NAME);
      if (setenv("XUL_APP_FILE", appEnv, 1)) 
      {
        NSLog(@"Couldn't set XUL_APP_FILE to: %s", appEnv);
        return 255;
      }
      NSLog(@"Set XUL_APP_FILE to: %s", appEnv);



      //CONSTRUCT GREDIR AND CALL XPCOMGLUE WITH IT
      char greDir[MAXPATHLEN];
      snprintf(greDir, MAXPATHLEN, "%s%s", [firefoxPath UTF8String], WEBAPPRT_PATH);
      isGreLoaded = NS_SUCCEEDED(AttemptGRELoad(greDir));


      if(!isGreLoaded) 
      {
        // TODO: User-friendly message explaining that FF needs to be installed
        return 255;
      }

      // NOTE: The GRE has successfully loaded, so we can use XPCOM now

      { // Scope for any XPCOM stuff we create
          nsINIParser parser;
          if(NS_FAILED(parser.Init(appEnv))) 
          {
            NSLog(@"%s was not found\n", appEnv);
            return 255;
          }


        // Get the path to the runtime's INI file.  This should be in the
        // same directory as the GRE.
        snprintf(rtINIPath, MAXPATHLEN, "%s%s%s", [firefoxPath UTF8String], WEBAPPRT_PATH, WEBRTINI_NAME);
        NSLog(@"webapprt.ini path: %s", rtINIPath);

        // Load the runtime's INI from its path.
        nsCOMPtr<nsILocalFile> rtINI;
        if(NS_FAILED(XRE_GetFileFromPath(rtINIPath, getter_AddRefs(rtINI)))) 
        {
          NSLog(@"Runtime INI path not recognized: '%s'\n", rtINIPath);
          return 255;
        }

        if(!rtINI) 
        {
          NSLog(@"Error: missing webapprt.ini");
          return 255;
        }

        nsXREAppData *webShellAppData;
        if (NS_FAILED(XRE_CreateAppData(rtINI, &webShellAppData))) 
        {
          NSLog(@"Couldn't read webapprt.ini: %s", rtINIPath);
          return 255;
        }

        char profile[MAXPATHLEN];
        if(NS_FAILED(parser.GetString("App", "Profile", profile, MAXPATHLEN))) 
        {
          NSLog(@"Unable to retrieve profile from web app INI file");
          return 255;
        }
        SetAllocatedString(webShellAppData->profile, profile);

        nsCOMPtr<nsILocalFile> directory;
        if(NS_FAILED(XRE_GetFileFromPath(greDir, getter_AddRefs(directory)))) 
        {
          NSLog(@"Unable to open app dir");
          return 255;
        }

        nsCOMPtr<nsILocalFile> xreDir;
        if(NS_FAILED(XRE_GetFileFromPath(greDir, getter_AddRefs(xreDir)))) 
        {
          NSLog(@"Unable to open XRE dir");
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
  
}
@catch (NSException *e) 
{
  NSLog(@"got exception: %@", e);
  displayErrorAlert([e name], [e reason]);
}
@finally 
{
  [pool drain];
  exit(1);
}

}


NSException* makeException(NSString* name, NSString* message) 
{
  NSException* myException = [NSException
        exceptionWithName:name
        reason:message
        userInfo:nil];
  return myException;
}

void displayErrorAlert(NSString* title, NSString* message)
{
  CFUserNotificationDisplayNotice(0, kCFUserNotificationNoteAlertLevel, 
    NULL, NULL, NULL, 
    (CFStringRef)title,
    (CFStringRef)message,
    CFSTR("Quit")
    );
}


/* Find the currently installed Firefox, if any, and return
 * an absolute path to it. may return nil */
NSString *pathToWebRT(NSString* alternateBinaryID)
{
  //default is firefox
  NSString *defaultBinary = @"org.mozilla.firefox";
  NSString *binaryPath = nil;

  //if they provided an override, try to find it
  if (alternateBinaryID != nil && ([alternateBinaryID length] > 0)) {
    binaryPath = [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:alternateBinaryID];
  }

  //if it isn't found, use firefox default instead
  if (binaryPath == nil || [binaryPath length] == 0) {
    binaryPath = [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:defaultBinary];
  }

  return binaryPath;
}


void execNewBinary(NSString* launchPath)
{

  NSLog(@" launching webrt at path: %@\n", launchPath);
  // char binfile[500];
  // sprintf(binfile, "%s/%s", WEBAPPRT_EXECUTABLE);

  const char *const newargv[] = {[launchPath UTF8String], NULL};

  NSLog(@"COMMAND LINE: '%@ %s'", launchPath, newargv[0]);
  execv([launchPath UTF8String], (char **)newargv);
}
