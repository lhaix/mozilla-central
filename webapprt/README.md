Build the branch using the normal mechanism.
`cd` into src/webapprt and run `make`

Create a directory in %APPDATA% according to the origin of your app:
  [subdomain.]server.domain;{http|https};port (or -1 if default)
  e.g. www.phoboslab.org;http;-1

The directory you created will be referred to as ${InstallDir}

In ${InstallDir}, create a file called "config.json".  It should have the following form:
  {
    "profile" : "full/path/to/profile/that/purchased/app",
    "origin" : "scheme://[subomain].domain.com"
  }

In ${InstallDir}, create a subdirectory named after the app (e.g. Z-Type).  It will be referred to as ${AppDir}

Copy webapprt.exe from dist/bin to ${AppDir}.

Create the following path: ${AppDir}/defaults/preferences/webapprt.js.  The file should have these contents:
  pref("browser.chromeURL","chrome://webapp/content/window.xul");
  pref("browser.hiddenWindowChromeURL", "chrome://webapp/content/hiddenWindow.xul");

In ${AppDir}, create an application.ini according to this template:
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

  [WebAppRT]
  FirefoxDir=Absolute path to dist/bin
