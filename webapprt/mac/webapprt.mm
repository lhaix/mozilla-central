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

void execNewBinary(NSString* launchPath);

NSString *pathToWebRT(NSString* alternateBinaryID);

NSException* makeException(NSString* name, NSString* message);

void displayErrorAlert(NSString* title, NSString* message);

//this is our version, to be compared with the version of the binary we are asked to use
NSString* myVersion = [NSString stringWithFormat:@"%s", NS_STRINGIFY(GRE_BUILDID)];

//we look for these flavors of Firefox, in this order
NSArray* launchBinarySearchList = [NSArray arrayWithObjects: @"org.mozilla.nightly", 
                                                              @"org.mozilla.aurora", 
                                                              @"org.mozilla.beta", 
                                                              @"org.mozilla.firefox", nil];


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
  NSString *firefoxPath = nil;   
  NSString *alternateBinaryID = nil;

  NSLog(@"MY WEBAPPRT BUILDID: %@", myVersion);

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];  

  //I need to look in our bundle first, before deciding what firefox binary to use
  NSBundle* myBundle = [NSBundle mainBundle];
  NSString* myBundlePath = [myBundle bundlePath];
  alternateBinaryID = [myBundle objectForInfoDictionaryKey:@"FirefoxBinary"];
  NSLog(@"found override firefox binary: %@", alternateBinaryID);

  @try 
  {
    //find a webapprt binary to launch with.  throws an exception with error dialog if none found.
    firefoxPath = pathToWebRT(alternateBinaryID);
    NSLog(@"USING FIREFOX : %@", firefoxPath);

    NSString *myWebRTPath = [myBundle pathForAuxiliaryExecutable: @"webapprt"];
    if (!myWebRTPath) 
    {
      @throw makeException(@"Missing WebRT Files", @"Cannot locate binary for this application");
    }

    //Check to see if the DYLD_FALLBACK_LIBRARY_PATH is set. if not, set it, and relaunch ourselves
    char libEnv[MAXPATHLEN];
    snprintf(libEnv, MAXPATHLEN, "%s%s", [firefoxPath UTF8String], WEBAPPRT_PATH);

    char* curVal = getenv("DYLD_FALLBACK_LIBRARY_PATH");

    if ((curVal == NULL) || strncmp(libEnv, curVal, MAXPATHLEN)) 
    {
      NSLog(@"DYLD_FALLBACK_LIBRARY_PATH NOT SET!!");
      //they differ, so set it and relaunch
      setenv("DYLD_FALLBACK_LIBRARY_PATH", libEnv, 1);
      execNewBinary(myWebRTPath);
      exit(0);
    }

    NSString *firefoxINIFilePath = [NSString stringWithFormat:@"%@%s%s", firefoxPath, WEBAPPRT_PATH, APPINI_NAME];
    nsINIParser ffparser;
    NSLog(@"Looking for firefox ini file here: %@", firefoxINIFilePath);

    if(NS_FAILED(ffparser.Init([firefoxINIFilePath UTF8String]))) 
    {
      NSLog(@"Unable to locate Firefox application.ini");
      @throw makeException(@"Error", @"Unable to parse environment files for application startup");
    }

    char ffVersChars[MAXPATHLEN];
    if(NS_FAILED(ffparser.GetString("App", "BuildID", ffVersChars, MAXPATHLEN))) 
    {
      NSLog(@"Unable to retrieve Firefox BuildID");
      @throw makeException(@"Error", @"Unable to determine Firefox version.");
    }
    NSString* firefoxVersion = [NSString stringWithFormat:@"%s", ffVersChars];

    NSLog(@"FIREFOX WEBAPPRT BUILDID: %@", firefoxVersion);

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

      int result = 0;
      char rtINIPath[MAXPATHLEN];

      // Set up our environment to know where application.ini was loaded from.
      char appEnv[MAXPATHLEN];
      snprintf(appEnv, MAXPATHLEN, "%s%s%s", [myBundlePath UTF8String], WEBAPPRT_PATH, APPINI_NAME);
      if (setenv("XUL_APP_FILE", appEnv, 1)) 
      {
        NSLog(@"Couldn't set XUL_APP_FILE to: %s", appEnv);
        @throw makeException(@"Error", @"Unable to set application INI file.");
      }
      NSLog(@"Set XUL_APP_FILE to: %s", appEnv);



      //CONSTRUCT GREDIR AND CALL XPCOMGLUE WITH IT
      char greDir[MAXPATHLEN];
      snprintf(greDir, MAXPATHLEN, "%s%s", [firefoxPath UTF8String], WEBAPPRT_PATH);
      if(!NS_SUCCEEDED(AttemptGRELoad(greDir))) 
      {
          @throw makeException(@"Error", @"Unable to load XUL files for application startup");
      }

      // NOTE: The GRE has successfully loaded, so we can use XPCOM now

      NS_LogInit();
      { // Scope for any XPCOM stuff we create
          nsINIParser parser;
          if(NS_FAILED(parser.Init(appEnv))) 
          {
            NSLog(@"%s was not found\n", appEnv);
            @throw makeException(@"Error", @"Unable to parse environment files for application startup");
          }


        // Get the path to the runtime's INI file.  This should be in the
        // same directory as the GRE.
        snprintf(rtINIPath, MAXPATHLEN, "%s%s%s", [firefoxPath UTF8String], WEBAPPRT_PATH, WEBRTINI_NAME);
        NSLog(@"webapprt.ini path: %s", rtINIPath);
        if (![[NSFileManager defaultManager] fileExistsAtPath:[NSString stringWithFormat:@"%s", rtINIPath]]) 
        {
          NSString* msg = [NSString stringWithFormat: @"This copy of Firefox (%@) cannot run web applications, because it is missing important files", firefoxVersion];
          @throw makeException(@"Missing WebRT Files", msg);
        }


        // Load the runtime's INI from its path.
        nsCOMPtr<nsILocalFile> rtINI;
        if(NS_FAILED(XRE_GetFileFromPath(rtINIPath, getter_AddRefs(rtINI)))) 
        {
          NSLog(@"Runtime INI path not recognized: '%s'\n", rtINIPath);
          @throw makeException(@"Error", @"Incorrect path to base INI file.");
        }

        if(!rtINI) 
        {
          NSLog(@"Error: missing webapprt.ini");
          @throw makeException(@"Error", @"Missing base INI file.");
        }

        nsXREAppData *webShellAppData;
        if (NS_FAILED(XRE_CreateAppData(rtINI, &webShellAppData))) 
        {
          NSLog(@"Couldn't read webapprt.ini: %s", rtINIPath);
          @throw makeException(@"Error", @"Unable to parse base INI file.");
        }

        char profile[MAXPATHLEN];
        if(NS_FAILED(parser.GetString("App", "Profile", profile, MAXPATHLEN))) 
        {
          NSLog(@"Unable to retrieve profile from web app INI file");
          @throw makeException(@"Error", @"Unable to retrieve installation profile.");
        }
        SetAllocatedString(webShellAppData->profile, profile);

        nsCOMPtr<nsILocalFile> directory;
        if(NS_FAILED(XRE_GetFileFromPath(greDir, getter_AddRefs(directory)))) 
        {
          NSLog(@"Unable to open app dir");
          @throw makeException(@"Error", @"Unable to open application directory.");
        }

        nsCOMPtr<nsILocalFile> xreDir;
        if(NS_FAILED(XRE_GetFileFromPath(greDir, getter_AddRefs(xreDir)))) 
        {
          NSLog(@"Unable to open XRE dir");
          @throw makeException(@"Error", @"Unable to open application XRE directory.");
        }

        xreDir.forget(&webShellAppData->xreDirectory);
        directory.forget(&webShellAppData->directory);

        // There is only XUL.
        result = XRE_main(argc, argv, webShellAppData);

        XRE_FreeAppData(webShellAppData);
      }
      NS_LogTerm();
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
  NSString *binaryPath = nil;

  //if they provided a manual override, use that.  If they made an error, it will fail to launch
  if (alternateBinaryID != nil && ([alternateBinaryID length] > 0)) 
  {
    binaryPath = [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:alternateBinaryID];
    if (binaryPath == nil || [binaryPath length] == 0) 
    {
      @throw makeException(@"WebAppRT Not Found", 
                            [NSString stringWithFormat:@"Failed to locate specified override runtime with signature '%@'", alternateBinaryID]);
    }
    return binaryPath;
  }

  //No override found, loop through the various flavors of firefox we have
  for (NSString* signature in launchBinarySearchList) {
    NSLog(@"SEARCHING for webapprt, trying: %@", signature);
    binaryPath = [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:signature];
    if (binaryPath && [binaryPath length] > 0) 
      return binaryPath;
  }

  NSLog(@"unable to find a valid webrt path");
  @throw makeException(@"Missing Runtime", @"Mozilla Apps require Firefox to be installed");

  return nil;
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
