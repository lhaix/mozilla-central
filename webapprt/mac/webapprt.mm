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
 * The Original Code is Open Web Apps for Firefox.
 *
 * The Initial Developer of the Original Code is The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Dan Walkowski <dwalkowski@mozilla.com>
 *    Michael Hanson <mhanson@mozilla.com>
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


//PLAN:

 // open my bundle, get the version number
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
//#include "nsWindowsWMain.cpp"            // we want a wmain entry point



const char *WEBAPPRT_EXECUTABLE = "webapprt";
const char *APPINI_NAME = "application.ini";
//need the correct relative path here
const char *WEBAPPRT_PATH = "/Contents/MacOS/"; 
const char *INFO_FILE_PATH = "/Contents/Info.plist";

void execNewBinary(NSString* launchPath);

NSString *pathToNewestFirefox(NSString* identifier);

NSException* makeException(NSString* name, NSString* message);

void displayErrorAlert(NSString* title, NSString* message);


int gVerbose = 0;

XRE_GetFileFromPathType XRE_GetFileFromPath;
XRE_CreateAppDataType XRE_CreateAppData;
XRE_FreeAppDataType XRE_FreeAppData;
XRE_mainType XRE_main;

  const nsDynamicFunctionLoad kXULFuncs[] = {
      { "XRE_GetFileFromPath", (NSFuncPtr*) &XRE_GetFileFromPath },
      { "XRE_CreateAppData", (NSFuncPtr*) &XRE_CreateAppData },
      { "XRE_FreeAppData", (NSFuncPtr*) &XRE_FreeAppData },
      { "XRE_main", (NSFuncPtr*) &XRE_main },
      { nsnull, nsnull }
  };

  nsresult AttemptGRELoad(char *greDir) {
    nsresult rv;
    char xpcomDLLPath[MAXPATHLEN];
    snprintf(xpcomDLLPath, MAXPATHLEN, "%s%s", greDir, XPCOM_DLL);

    rv = XPCOMGlueStartup(xpcomDLLPath);

    if(NS_FAILED(rv)) {
      return rv;
    }

    rv = XPCOMGlueLoadXULFunctions(kXULFuncs);
    if(NS_FAILED(rv)) {
      return rv;
    }

    return rv;
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


int main(int argc, char **argv)
{
  int i;
  NSString *firefoxPath = NULL;   
  NSString *bundleID = NULL;

  for (i=1;i < argc;i++)
  {
    if (!strcmp(argv[i], "-v")) {
      gVerbose = 1;
    } else if (!strcmp(argv[i], "-b")) {
      if (i+1 < argc) {
        bundleID = [NSString stringWithFormat: @"%s", argv[i+1]];
        i++;
      }
    }
  }


  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];  

  @try {

  if (!firefoxPath) {
    firefoxPath = pathToNewestFirefox(bundleID); // XXX developer feature to specify other firefox here
    if (!firefoxPath) {
      // Launch a dialog to explain to the user that there's no compatible web runtime
      @throw makeException(@"Missing Web Runtime", @"Web Applications require Firefox to be installed");
    }
  }


  //Get the version number from my Info.plist.  This is more complicated that you might think; the information isn't meant for us,
  // it's meant for the OS.  We're supposed to know our version number.  I am reading the file and parsing into a mutable
  // dictionary object so that we can update it if necessary with the current version number and write it out again.
  NSString* myBundlePath = [[NSBundle mainBundle] bundlePath];
  NSString* myInfoFilePath = [NSString stringWithFormat:@"%@%s", myBundlePath, INFO_FILE_PATH];
  if (![[NSFileManager defaultManager] fileExistsAtPath:myInfoFilePath]) {
    //get out of here, I don't have a bundle file?
    NSLog(@"webapp bundle file not found at path: %@", myInfoFilePath);
    @throw makeException(@"Invalid App Package", @"This App appears to be damaged, please reinstall it");
  }
  //get writable copy of my Info.plist file as a dictionary
  NSData *myAppData = [[NSData alloc] initWithContentsOfFile:myInfoFilePath];

  NSError *errorDesc = nil;
  NSMutableDictionary* myAppInfo = 
          (NSMutableDictionary *)[NSPropertyListSerialization propertyListWithData: myAppData 
                                                              options: NSPropertyListMutableContainersAndLeaves
                                                              format: NULL 
                                                              error: &errorDesc];
  if (errorDesc != nil) {
    //we failed to deserialize the property list, so we likely need to bail
    NSLog(@"unable to deserialize webapp property list: %@", errorDesc);
    @throw makeException(@"Unable To Read App Version", @"This App appears to be damaged, please reinstall it");
  }
  //ok now read my version string
  NSString* myVersion = [myAppInfo objectForKey:@"CFBundleVersion"];
  NSLog(@"WebappRT version found: %@", myVersion);
  [myAppData release];
  //Finished getting my current version


  //now get Firefox version, but read-only, which is much simpler.
  NSString* firefoxInfoFile = [NSString stringWithFormat:@"%@%s", firefoxPath, INFO_FILE_PATH];
  if (![[NSFileManager defaultManager] fileExistsAtPath:firefoxInfoFile]) {
    //get out of here, I don't have a bundle file?
    NSLog(@"Firefox webapprt bundle file not found at path: %@", firefoxInfoFile);
    @throw makeException(@"Missing WebRT Files", @"This version of Firefox cannot run web applications, because it is not recent enough or damaged");
  }

  //get read-only copy of FF Info.plist file
  NSDictionary *firefoxAppInfo = [NSDictionary dictionaryWithContentsOfFile:firefoxInfoFile];
  if (!firefoxAppInfo) {
    @throw makeException(@"Unreadable Info File", @"This version of Firefox cannot run web applications, because it is not recent enough or damaged");
  }
  NSString* firefoxVersion = [firefoxAppInfo objectForKey:@"CFBundleVersion"];
  //Finished getting Firefox version


  //compare them
  if ([myVersion compare: firefoxVersion] != NSOrderedSame) {
    //we are going to assume that if they are different, we need to re-copy the webapprt, regardless of whether
    // it is newer or older.  If we don't find a webapprt, then the current Firefox must not be new enough to run webapps.
    NSLog(@"This Application has an old webrt. Updating it.");

    //we know the firefox path, so copy the new webapprt here
    NSString *newWebRTPath = [NSString stringWithFormat: @"%@%s%s", firefoxPath, WEBAPPRT_PATH, WEBAPPRT_EXECUTABLE];
    NSLog(@"firefox webrt path: %@", newWebRTPath);
    if (![[NSFileManager defaultManager] fileExistsAtPath:newWebRTPath]) {
      @throw makeException(@"Missing WebRT Files", @"This version of Firefox cannot run web applications, because it is not recent enough or damaged");
    }

    NSString *myWebRTPath = [NSString stringWithFormat: @"%@%s%s", myBundlePath, WEBAPPRT_PATH, WEBAPPRT_EXECUTABLE];
    NSLog(@"my webrt path: %@", myWebRTPath);
    if (![[NSFileManager defaultManager] fileExistsAtPath:myWebRTPath]) {
      @throw makeException(@"Missing WebRT Files", @"Cannot locate binary for this application");
    }
        //unlink my binary file
    int err = unlink([myWebRTPath UTF8String]);
    if (err) {
      NSLog(@"failed to unlink old binary file at path: %@", myWebRTPath);
      @throw makeException(@"Unable To Update", @"Failed preparation for runtime update");
    }

    NSFileManager* clerk = [[NSFileManager alloc] init];
    [clerk copyItemAtPath: newWebRTPath toPath: myWebRTPath error: &errorDesc];
    [clerk release];
    if (errorDesc != nil) {
      NSLog(@"failed to copy new webrt file");
      @throw makeException(@"Unable To Update", @"Failed to update runtime");
    }

    //If we successfully copied over the newer webapprt, then overwrite my version number with the firefox version number,
    // and save the dictionary out as my new Info.plist
    [myAppInfo setObject: firefoxVersion forKey: @"CFBundleVersion"];

    NSData* newProps = [NSPropertyListSerialization dataWithPropertyList:myAppInfo format: NSPropertyListXMLFormat_v1_0 options:0 error: &errorDesc];
    BOOL success = [newProps writeToFile: myInfoFilePath atomically: YES];
    if (!success) {
      NSLog(@"Failed to write out updated Info.plist: %@", errorDesc);
      @throw makeException(@"Version Update Failure", @"Failed to update the app version number to the current version");
    }

    //execv the new binary, and ride off into the sunset
    execNewBinary(myWebRTPath);

  } else {
    //we are ready to load XUL and such, and go go go

      NSLog(@"This Application has the newest webrt.  Launching!");
      bool isGreLoaded = false;

      int result = 0;
      char appINIPath[MAXPATHLEN];
      char rtINIPath[MAXPATHLEN];
      
      //CONSTRUCT GREDIR AND CALL XPCOMGLUE WITH IT
      char greDir[MAXPATHLEN];
      snprintf(greDir, MAXPATHLEN, "%s%s", [firefoxPath UTF8String], WEBAPPRT_PATH);
      isGreLoaded = NS_SUCCEEDED(AttemptGRELoad(greDir));


      if(!isGreLoaded) {
        // TODO: User-friendly message explaining that FF needs to be installed
        return 255;
      }

      // NOTE: The GRE has successfully loaded, so we can use XPCOM now

      { // Scope for any XPCOM stuff we create
          nsINIParser parser;
          if(NS_FAILED(parser.Init(appINIPath))) {
            NSLog(@"%s was not found\n", appINIPath);
            return 255;
          }


        // Set up our environment to know where application.ini was loaded from.
        char appEnv[MAXPATHLEN];
        snprintf(appEnv, MAXPATHLEN, "XUL_APP_FILE=%s%s", [myBundlePath UTF8String], APPINI_NAME);
        if (putenv(appEnv)) {
          NSLog(@"Couldn't set %s.\n", appEnv);
          return 255;
        }

        // Get the path to the runtime's INI file.  This should be in the
        // same directory as the GRE.
        snprintf(rtINIPath, MAXPATHLEN, "%s%s", [firefoxPath UTF8String], APPINI_NAME);

        // Load the runtime's INI from its path.
        nsCOMPtr<nsILocalFile> rtINI;
        if(NS_FAILED(XRE_GetFileFromPath(rtINIPath, getter_AddRefs(rtINI)))) {
          NSLog(@"Runtime INI path not recognized: '%s'\n", rtINIPath);
          return 255;
        }

        if(!rtINI) {
          NSLog(@"Error: missing webapprt.ini");
          return 255;
        }

        nsXREAppData *webShellAppData;
        if (NS_FAILED(XRE_CreateAppData(rtINI, &webShellAppData))) {
          NSLog(@"Couldn't read webapprt.ini\n");
          return 255;
        }

        char profile[MAXPATHLEN];
        if(NS_FAILED(parser.GetString("App", "Profile", profile, MAXPATHLEN))) {
          NSLog(@"Unable to retrieve profile from web app INI file");
          return 255;
        }
        SetAllocatedString(webShellAppData->profile, profile);

        nsCOMPtr<nsILocalFile> directory;
        if(NS_FAILED(XRE_GetFileFromPath(greDir, getter_AddRefs(directory)))) {
          NSLog(@"Unable to open app dir");
          return 255;
        }

        nsCOMPtr<nsILocalFile> xreDir;
        if(NS_FAILED(XRE_GetFileFromPath(greDir, getter_AddRefs(xreDir)))) {
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
@catch (NSException *e) {
  NSLog(@"got exception: %@", e);
  displayErrorAlert([e name], [e reason]);
}
@finally {
  [pool drain];
  exit(1);
}

}


NSException* makeException(NSString* name, NSString* message) {
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
 * an absolute path to it. */
NSString *pathToNewestFirefox(NSString* identifier)
{
  //default is firefox
  NSString* appIdent = @"org.mozilla.nightlydebug";
  if (identifier != NULL) appIdent = identifier;

  NSString *firefoxRoot = [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:appIdent];
  if (firefoxRoot) {
    return firefoxRoot;
  } else {
    return NULL;
  }
}


void execNewBinary(NSString* launchPath)
{

  NSLog(@" launching webrt at path: %@\n", launchPath);
  char binfile[100];
  sprintf(binfile, "%s", WEBAPPRT_EXECUTABLE);

  char *newargv[2];
  newargv[0] = binfile;
  newargv[1] = NULL;

  NSLog(@"COMMAND LINE: '%@ %s'", launchPath, newargv[0]);
  execv([launchPath UTF8String], (char **)newargv);
}
