Build Instructions
==================
- Build Firefox using the normal mechanism.

Windows Native App Installation Flow
====================================
**PORT** Create an app-specific directory according
to the origin of your app.

  - Location: `%APPDATA%`
  - Name: [subdomain.]server.domain;{http|https};port (or -1 if default)
  - e.g. `%APPDATA%\www.phoboslab.org;http;-1`
  - Impl note: The pieces of the directory name can be retrieved from an
  `nsIURI` whose spec is the origin of the app.

The app-specific directory will be referred to as `${InstallDir}`.

**TODO** Create the file `${InstallDir}/config.json` with contents:

  - "profile" : A string containing the full path to the profile that purchased
  the app.
  - e.g. `"C:\Users\tabraldes\AppData\Roaming\Mozilla\Firefox\Profiles\
  qq5c1oaf.default"`
  - "origin" : A string containing the origin of the app.
  - e.g. `http://www.phoboslab.org` or `http://127.0.0.1:1234`

**TODO** Create the directory `${InstallDir}\bin`.

`${InstallDir}\bin` will be referred to as `${AppDir}`.  
The dir of the currently running Firefox will be referred to as `${Firefox}`.  
The name of the app, sanitized to work as a Windows filename,
will be referred to as `${AppName}`.  

**TODO** Copy `${Firefox}\webapprt.exe` to `${AppDir}\${AppName}.exe`.

**TODO** Create the file `${AppDir}\application.ini`
according to this template:  

    # ==== Windows application.ini template ====
    [App]
    Name=${AppName}
    # Just the dir name, not the full path
    # e.g. www.phoboslab.org;http;-1
    Profile=${InstallDir}
    
    [WebAppRT]
    # Full path to directory of currently running Firefox
    # e.g. C:\program files\Nightly
    FirefoxDir=${Firefox}

**PORT** Retrieve the largest icon specified in the app's manifest.  
**PORT** Convert the icon to .ICO format (The current addon does this).  
**PORT** Write the converted icon to `${AppDir}\${AppName}.ico`.  
**TODO** Embed the icon in `${AppDir}\${AppName}.exe`.

  - Initially, we may want to accomplish this by using *redit.exe*.
Redit.exe is built when xulrunner is built, and its sole purpose is
to embed icons in executable files.  We will have to package redit.exe
with Firefox if we use this approach.
  - Longer term, we may want to consider porting redit.exe to javascript
using js-ctypes.

**PORT** Write the uninstall registry key for the app and all of its values.

**PORT** Run the webapp installer executable.  
The installer is responsible for

  - Generating `${AppDir}\${AppName}.lnk`, a shortcut to the app.
  - Writing `${AppDir}\uninstall.exe`

**PORT** Copy `${AppDir}\${AppName}.lnk` to the user's Desktop and Start Menu

Mac Native Installation Flow
============================
**TODO** Write this
