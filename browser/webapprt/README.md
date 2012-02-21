Build the branch

Create the webappshell directory: dist/bin/webappshell.
Copy browser/app/webapp/WebAppService.jsm (note that this is from your source dir) into dist/bin/modules.
Copy dist/bin/greprefs.js into dist/bin/webappshell.
Copy dist/bin/chrome.manifest into dist/bin/webappshell.
In dist/bin/webappshell/chrome.manifest:
  Replace BrowserComponents.manifest with WebappShellComponents.manifest
  Remove the line "manifest chrome/browser.manifest"
  Add "content webapp content/" (without quotes) as the last line
Copy dist/bin/components/BrowserComponents.manifest into new file dist/bin/components/WebappShellComponents.manifest.
In dist/bin/WebappShellComponents.manifest, replace all instances of the GUID that appears after "application=" with {fc882682-1f34-4d82-9dc7-5f31f14f623e}.
Symlink the following directories into dist/bin/webappshell:
  dist/bin/chrome
  dist/bin/components
  dist/bin/modules
  dist/bin/res
Copy the directory app/webapp/content (note that this is from your source dir) into dist/bin/webappshell.
Copy the directory dist/bin/defaults to dist/bin/webappshell.
Rename the file dist/bin/webappshell/defaults/pref/firefox.js to webappshell.js.
In dist/bin/webappshell/defaults/pref/webappshell.js:
  Change chromeURL to chrome://webapp/content/window.xul
  Change hiddenWindowChromeURL to chrome://webapp/content/hiddenWindow.xul

Create a directory in %APPDATA% according to the origin of your app:
  [subdomain.]server.domain;{http|https};port (or -1 if default)
  e.g. www.phoboslab.org;http;-1

Create a file called "config.json" in the app-specific directory.  It should have the following form:
  {
    "profile" : "full/path/to/profile/that/purchased/app",
    "origin" : "scheme://[subomain].domain.com"
  }

Create a subdirectory of the app-specific directory named after the app (e.g. Z-Type).
Copy apprt.exe from dist/bin to the subdirectory you just created.
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
FirefoxDir=Absolute path to dist/bin
