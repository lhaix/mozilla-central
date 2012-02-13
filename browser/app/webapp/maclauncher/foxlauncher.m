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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>

const char *ENVIRONMENT_DIR = "env";
const char *FIREFOX_EXECUTABLE = "firefox-bin";
const char *VERSION_FILE = "ffx.version";

void launchApplication(NSString* path);

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
  if (!firefoxPath) {
    firefoxPath = pathToCurrentFirefox(bundleID); // XXX developer feature to specify other firefox here
    if (!firefoxPath) {
      // Launch a dialog to explain to the user that there's no compatible web runtime
      displayErrorAlert(CFSTR("Cannot start"),
        CFSTR("Cannot start application.  This application requires that Firefox be installed.\n\nDownload it from http://getfirefox.com"));
      return 0;
    }
  }
  
  launchApplication(firefoxPath);
  [pool drain];
  exit(1);
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



void launchApplication(NSString* path)
{
  const char *appdir = [[[NSBundle mainBundle] bundlePath] UTF8String];

  char launchPath[1024];
  snprintf(launchPath, 1024, "%s/Contents/MacOS/%s", [path UTF8String], FIREFOX_EXECUTABLE);
  NSLog(@"#####foxlauncher lauching firefox at path: %s\n", launchPath);

  char appIniPath[1024];
  snprintf(appIniPath, 1024, "%s/Contents/application.ini", appdir);
  NSLog(@"#####foxlauncher using app path: %s\n", appIniPath);

  char *newargv[4];
  newargv[0] = "firefox-bin";
  newargv[1] = "-webapp";
  newargv[2] = appIniPath;
  newargv[3] = NULL;

  NSLog(@"COMMAND LINE: '%s %s %s %s'", launchPath, newargv[0], newargv[1], newargv[2]);
  execv(launchPath, (char **)newargv);
}
