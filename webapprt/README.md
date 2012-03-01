Build Instructions
==================
- Build Firefox using the normal mechanism.

Windows Native App Installation Flow
====================================
**PORT** Create an app-specific directory according
to the origin of the app.

  - Location: `%APPDATA%`
  - Name: [subdomain.]server.domain;{http|https};port (or -1 if default)
  - e.g. `%APPDATA%\www.phoboslab.org;http;-1`
  - Impl note: The pieces of the directory name can be retrieved from an
  `nsIURI` whose spec is the origin of the app.

The app-specific directory will be referred to as `${InstallDir}`.

**TODO** Create the file `${InstallDir}/config.json` with contents:

  - "profile" : A string containing the full path to the
  FF profile that purchased the app.  This is the currently
  active profile. e.g.
  `"C:\Users\myuser\AppData\Roaming\Mozilla\Firefox\Profiles\qq5c1oaf.default"`
  - "origin" : A string containing the origin of the app.
  e.g. `http://www.phoboslab.org` or `http://127.0.0.1:1234`

**TODO** Create the directory `${InstallDir}\bin`.

`${InstallDir}\bin` will be referred to as `${AppDir}`.  
The dir of the currently running Firefox will be referred to as `${Firefox}`.  
The name of the app will be referred to as `${AppDisplayName}`.  
The name of the app, sanitized to work as a Windows filename,
will be referred to as `${AppFilename}`.  

**TODO** Copy `${Firefox}\webapprt.exe` to `${AppDir}\${AppFilename}.exe`.

**TODO** Create the file `${AppDir}\application.ini`
according to this template (IMPL NOTE: See
[nsIINIParserWriter](https://mxr.mozilla.org/mozilla-central/source/xpcom/ds/nsIINIParser.idl#63)
and
[nsINIProcessor](https://mxr.mozilla.org/mozilla-central/source/xpcom/ds/nsINIProcessor.js#63)):

    # ==== Windows application.ini template ====
    [App]
    # NOTE: Slashes in the display name will cause the app to fail to launch
    Name=${AppDisplayName}
    # Just the dir name, not the full path
    # e.g. www.phoboslab.org;http;-1
    Profile=${InstallDir}
    
    [WebAppRT]
    # Full path to directory of currently running Firefox
    # e.g. C:\program files\Nightly
    FirefoxDir=${Firefox}

**PORT** Retrieve the largest icon specified in the app's manifest.  
**PORT** Convert the icon to .ICO format  
**PORT** Write the converted icon to
`${InstallDir}\chrome\icons\default\topwindow.ico`.

**TODO** Embed the icon in `${AppDir}\${AppFilename}.exe`.

  - Initially, we may want to accomplish this by using **redit.exe**.
Originally designed as a utility to aid in the deployment of xulrunner apps,
redit.exe's sole purpose is to embed icons in executable files.  Redit
has been copied into the webapprt source directory, so it will be built when
webapprt is built.  Using this approach means that redit.exe must be packaged
with Firefox.
  - Longer term, we may want to consider porting redit.exe to javascript
using js-ctypes.

**PORT** Write the uninstall registry key and its values.
Specifically, we'll want to make sure we write the following values:

  - **DisplayIcon** (String) Full path to `topwindow.ico`
  - **DisplayName** (String) ${AppDisplayName}
  - **InstallLocation** (String) Full path to `${InstallDir}`
  - **NoModify** (Int) 1
  - **NoRepair** (Int) 1
  - **AppFilename** (String) `${AppFilename}`
  - **UninstallString** See the addon implementation for how to generate this

**PORT** Run the webapp installer executable.  
The installer is responsible for

  - Generating `${AppDir}\${AppFilename}.lnk`, a shortcut to the app.
  - Writing `${AppDir}\uninstall.exe`

**PORT** Copy `${AppDir}\${AppFilename}.lnk` to the user's Desktop and Start Menu

Mac Native Installation Flow
============================
**TODO** Write this
