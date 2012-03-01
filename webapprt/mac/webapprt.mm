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

const char *WEBAPPRT_EXECUTABLE = "webapprt";
//need the correct relative path here
const char *WEBAPPRT_PATH = "/Contents/MacOS/"; 

void execNewBinary(NSString* launchPath);

NSString *pathToCurrentFirefox(NSString* identifier);

void displayErrorAlert(CFStringRef title, CFStringRef message);


int gVerbose = 0;


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
        bundleID = [[NSString alloc] initWithCString:argv[i+1]];
        i++;
      }
    }
  }

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];  

  @try {

  if (!firefoxPath) {
    firefoxPath = pathToCurrentFirefox(bundleID); // XXX developer feature to specify other firefox here
    if (!firefoxPath) {
      // Launch a dialog to explain to the user that there's no compatible web runtime
      displayErrorAlert(CFSTR("Cannot start"),
        CFSTR("Cannot start application.  This application requires that Firefox be installed.\n\nDownload it from http://getfirefox.com"));
      return 0;
    }
  }


  //Get the version number from my Info.plist.  This is more complicated that you might think; the information isn't meant for us,
  // it's meant for the OS.  We're supposed to know our version number.  I am reading the file and parsing into a mutable
  // dictionary object so that we can update it if necessary with the current version number and write it out again.
  NSString* myBundlePath = [[NSBundle mainBundle] bundlePath];
  NSString* myInfoFilePath = [NSString stringWithFormat:@"%@/Info.plist", myBundlePath];
  if (![[NSFileManager defaultManager] fileExistsAtPath:myInfoFilePath]) {
    //get out of here, I don't have a bundle file?
    NSLog(@"webapp bundle file not found at path: %@", myInfoFilePath);
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
    //exit probably
  }
  //ok now read my version string
  NSString* myVersion = [myAppInfo objectForKey:@"CFBundleVersion"];
  NSLog(@"WebappRT version found: %@", myVersion);
  [myAppData release];
  //Finished getting my current version


  //now get Firefox version, but read-only, which is much simpler.
  NSString* firefoxInfoFile = [NSString stringWithFormat:@"%@/Info.plist", firefoxPath];
  //get read-only copy of FF Info.plist file
  NSDictionary *firefoxAppInfo = [NSDictionary dictionaryWithContentsOfFile:firefoxInfoFile];
  NSString* firefoxVersion = [firefoxAppInfo objectForKey:@"CFBundleVersion"];
  //Finished getting Firefox version


  //compare them
  if ([myVersion compare: firefoxVersion] != NSOrderedSame) {
    //we are going to assume that if they are different, we need to re-copy the webapprt, regardless of whether
    // it is newer or older.  If we don't find a webapprt, then the current Firefox must not be new enough to run webapps.

    //unlink my binary file
    int err = unlink(argv[0]);
    if (err) {
      NSLog(@"failed to unlink old binary file at path: %s", argv[0]);
      //exit, probably
    }

    //we know the firefox path, so copy the new webapprt here
    NSString *newWebRTPath = [NSString stringWithFormat: @"%@%@%@", firefoxPath, WEBAPPRT_PATH, WEBAPPRT_EXECUTABLE];
    NSLog(@"firefox webrt path: %@", newWebRTPath);
    NSString *myWebRTPath = [NSString stringWithFormat: @"%@%@%@", myBundlePath, WEBAPPRT_PATH, WEBAPPRT_EXECUTABLE];
    NSLog(@"my webrt path: %@", myWebRTPath);

    NSFileManager* clerk = [[NSFileManager alloc] init];
    [clerk copyItemAtPath: newWebRTPath toPath: myWebRTPath error: &errorDesc];
    [clerk release];
    if (errorDesc != nil) {
      NSLog(@"failed to copy new webrt file");
      //exit, probably
    }

    //If we successfully copied over the newer webapprt, then overwrite my version number with the firefox version number,
    // and save the dictionary out as my new Info.plist
    [myAppInfo setObject: firefoxVersion forKey: @"CFBundleVersion"];

    NSData* newProps = [NSPropertyListSerialization dataWithPropertyList:myAppInfo format: NSPropertyListXMLFormat_v1_0 options:0 error: &errorDesc];
    BOOL success = [newProps writeToFile: myInfoFilePath atomically: YES];
    if (!success) {
      NSLog(@"Failed to write out updated Info.plist: %@", errorDesc);
      //exit, probably
    }

    //execv the new binary, and ride off into the sunset
    execNewBinary(myWebRTPath);

  }
  else {
    //we are ready to load XUL and such, and go go go

  }
  
}
@catch (NSException *e) {
  NSLog(@"got exception: %@", e);
}
@finally {
  [pool drain];
  exit(1);
}

}


void displayErrorAlert(CFStringRef title, CFStringRef message)
{
  CFUserNotificationDisplayNotice(0, kCFUserNotificationNoteAlertLevel, 
    NULL, NULL, NULL, 
    title,
    message,
    CFSTR("Quit")
    );
}


/* Find the currently installed Firefox, if any, and return
 * an absolute path to it. */
NSString *pathToCurrentFirefox(NSString* identifier)
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

  NSLog(@" lauching webrt at path: %s\n", launchPath);
  char binfile[100];
  sprintf(binfile, "%s", WEBAPPRT_EXECUTABLE);

  char *newargv[2];
  newargv[0] = binfile;
  newargv[1] = NULL;

  NSLog(@"COMMAND LINE: '%s %s'", launchPath, newargv[0]);
  execv([launchPath UTF8String], (char **)newargv);
}
