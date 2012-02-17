Create a mozconfig that can build both the "apprt" and "browser" projects:
  mk_add_options MOZ_BUILD_PROJECTS="apprt browser"
Note that doing this makes it so you cannot specify a relative path to "client.mk" in your make command.  You instead have to do something like "make -f full/path/to/client.mk"

Build the branch.  This should create two subdirectories in your objdir: apprt and browser.

Create the webappshell directory: browser/dist/bin/webappshell.
Copy browser/dist/bin/greprefs.js into browser/dist/bin/webappshell.
Copy browser/dist/bin/chrome.manifest into browser/dist/bin/webappshell.
In browser/dist/bin/webappshell/chrome.manifest:
  Replace BrowserComponents.manifest with WebappShellComponents.manifest
  Remove the line "manifest chrome/browser.manifest"
  Add "content webapp content/" (without quotes) as the last line
Copy browser/dist/bin/components/BrowserComponents.manifest into new file browser/dist/bin/components/WebappShellComponents.manifest.
In browser/dist/bin/WebappShellComponents.manifest, replace all instances of the GUID that appears after "application=" with {fc882682-1f34-4d82-9dc7-5f31f14f623e}.
Symlink the following directories into browser/dist/bin/webappshell:
  browser/dist/bin/chrome
  browser/dist/bin/components
  browser/dist/bin/modules
  browser/dist/bin/res
Copy the directory browser/app/webapp/content (note that this is from your source dir) into browser/dist/bin/webappshell.
Copy the directory browser/dist/bin/defaults to browser/dist/bin/webappshell.
Rename the file browser/dist/bin/webappshell/defaults/pref/firefox.js to webappshell.js.
In browser/dist/bin/webappshell/defaults/pref/webappshell.js:
  Change chromeURL to chrome://webapp/content/window.xul
  Change hiddenWindowChromeURL to chrome://webapp/content/hiddenWindow.xul
Copy apprt.exe from apprt/dist/bin to browser/dist/bin.

Create a directory in %APPDATA% according to the origin of your app:
  [subdomain.]server.domain;{http|https};port (or -1 if default)
  e.g. www.phoboslab.org;http;-1

Create a file called "origin" in the app-specific directory.  Its entire contents should be a single line that is the origin of the app (e.g. http://www.phoboslab.org).
Create a subdirectory of the app-specific directory named after the app (e.g. Z-Type).
Copy apprt.exe from apprt/dist/bin to the subdirectory you just created.
Create an application.ini inside the same subdirectory according to this template:

[App]
Vendor=Doesn't matter
Name=Name of your app (e.g. Z-Type)
Version=Doesn't matter
BuildID=Doesn't matter
ID={fc882682-1f34-4d82-9dc7-5f31f14f623e}
Profile=Name of the app-specific directory you created

[Gecko]
MinVersion=9.*
MaxVersion=13.*

[AppRT]
FirefoxDir=Absolute path to browser/dist/bin
