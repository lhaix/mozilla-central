Windows Native App Installation Flow
====================================
**DONE** Create an app-specific directory according
to the origin of the app.

  - Location: `%APPDATA%`
  - Name: [subdomain.]server.domain;{http|https};port (or -1 if default)
  - e.g. `%APPDATA%\www.phoboslab.org;http;-1`
  - Impl note: The pieces of the directory name can be retrieved from an
  `nsIURI` whose spec is the origin of the app.

The app-specific directory will be referred to as `${InstallDir}`.

**DONE** Create the file `${InstallDir}/config.json` with contents:

  - "profile" : A string containing the full path to the
  FF profile that purchased the app.  This is the currently
  active profile. e.g.
  `"C:\Users\myuser\AppData\Roaming\Mozilla\Firefox\Profiles\qq5c1oaf.default"`
  - "origin" : A string containing the origin of the app.
  e.g. `http://www.phoboslab.org` or `http://127.0.0.1:1234`

The dir of the currently running Firefox will be referred to as `${Firefox}`.  
The name of the app will be referred to as `${AppDisplayName}`.  
The name of the app, sanitized to work as a Windows filename,
will be referred to as `${AppFilename}`.  

**DONE** Copy `${Firefox}\webapprt.exe` to `${InstallDir}\${AppFilename}.exe`.

**TODO** Create the file `${InstallDir}\application.ini`
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
    Filename=${AppFilename}
    
    [Branding]
    BrandFullName=${AppDisplayName}
    BrandShortName=${AppDisplayName}

**DONE** Retrieve the largest icon specified in the app's manifest.  
**DONE** Convert the icon to .ICO format  
**DONE** Write the converted icon to
`${InstallDir}\chrome\icons\default\topwindow.ico`.

**DONE** Embed the icon in `${InstallDir}\${AppFilename}.exe`.

  - Initially, we may want to accomplish this by using **redit.exe**.
Originally designed as a utility to aid in the deployment of xulrunner apps,
redit.exe's sole purpose is to embed icons in executable files.  Redit
has been copied into the webapprt source directory, so it will be built when
webapprt is built.  Using this approach means that redit.exe must be packaged
with Firefox.
  - Longer term, we may want to consider porting redit.exe to javascript
using js-ctypes.

**TODO** Create `${AppFilename}.lnk`, a shortcut to the app,
on the user's Desktop and Start Menu.
To accomplish this, call the `setShortcut` function available in
`webapprt\modules\WindowsShortcutService.jsm`.

**TODO** Create the directory `${InstallDir}\uninstall`.  This directory
will be referred to as ${UninstallDir}.

**TODO** Copy `${Firefox}\webapp-uninstaller.exe` to
`${UninstallDir}`.

**TODO** Create the file `${UninstallDir}\shortcuts_log.ini`
according to this template:

    # ==== shortcuts_log.ini template ====
    [STARTMENU]
    Shortcut0=${AppFilename}.lnk
    [DESKTOP]
    Shortcut0=${AppFilename}.lnk
    [TASKBAR]
    Migrated=true

**TODO** Copy `${Firefox}\webapp-uninstall.log` to
`${UninstallDir}\uninstall.log`

**TODO** Write the uninstall registry key and its values.
The uninstall registry key is the full origin of the app, including port
(or -1 if default). For example:
`HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\http://www.myexample.com:-1`.
Specifically, we'll want to make sure we write the following values:

  - **DisplayIcon** (String) Full path to `topwindow.ico`
  - **DisplayName** (String) ${AppDisplayName}
  - **InstallLocation** (String) Full path to `${InstallDir}`
  - **NoModify** (Int) 1
  - **NoRepair** (Int) 1
  - **UninstallString** (String) `${UninstallDir}\webapp-uninstaller.exe`

Mac Native Installation Flow
============================

Tasks:

 1) Create Application Support Directory
 2) Create Application Directory


 Step 1: Create Application Support Directory
 
  - Create folder:  ~/Library/Application Support/${profile}, where ${profile} 
    is a unique identifying string for this application. The pattern used above 
    for Windows is fine, e.g. www.phoboslab.org;http;-1.  Those are all legal filename chars in MacOS.
  
    - Create file: ~/Library/Application Support/${profile}/config.json, which is identical to the
      config.json described above for Windows


Step 2:  note: all folder and file names are case-sensitive!

  - Create folder: /Applications/${appname}, where ${appname} is the friendly, user-facing name of
    the application, which is in the install record.

    - Create folder: /Applications/${appname}/Contents

      - Create folder: /Applications/${appname}/Contents/MacOS
        
        - Copy file: Firefox.app/Contents/MacOS/webapprt to 
            /Applications/${appname}/Contents/MacOS/webapprt

        - Create file: application.ini
        
          - The template is similar to the Windows example above, but doesn't need the firefox path:  
              [App]  
              Name=${appname}  
              Profile=${profile}  
        
        - Create folder: /Applications/${appname}/Contents/Resources
          - Create file: /Applications/${appname}/Contents/Resources/appicon.icns
              - this is a binary file, there is example code in nativeshell.js, which calls /usr/bin/sips to create the file from 
                the icon(s) specified in the app manifest.
                https://github.com/mozilla/openwebapps/blob/develop/addons/jetpack/lib/nativeshell.js

 

      - Create file: /Applications/${appname}/Contents/Info.plist
          - The Info.plist file is a metadata file necessary for all Mac native apps.
            It can be in one of several interchangeable formats, but we will only create
            xml formatted ones. All ${xxx} must be substituted. 
            
            Here is a template file:
              
                <?xml version="1.0" encoding="UTF-8"?>  
                
                <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">  
                <plist version="1.0">  
                <dict>  
                  <key>CFBundleDevelopmentRegion</key>  
                  <string>English</string>  
    	            <key>CFBundleDisplayName</key>  
    	            <string>${appname}</string>  
    	            <key>CFBundleExecutable</key>  
    	            <string>webapprt</string>  
    	            <key>CFBundleIconFile</key>  
    	            <string>appicon</string>  
    	            <key>CFBundleIdentifier</key>  
    	            <string>${origin}</string>  
    	            <key>CFBundleInfoDictionaryVersion</key>  
    	            <string>6.0</string>  
    	            <key>CFBundleName</key>  
    	            <string>${appname}</string>  
    	            <key>CFBundlePackageType</key>  
    	            <string>APPL</string>  
    	            <key>CFBundleSignature</key>  
    	            <string>MOZB</string>  
    	            <key>CFBundleVersion</key>  
    	            <string>${firefoxversion}</string>   
    	            
    	            OPTIONAL FOR DEVELOPERS:
                    <key>FirefoxBinary</key>    
	            <string>$EMPTY | org.mozilla.${aurora|auroradebug|nightly|nightdebug|etc}</string>   
	            
                </dict>  
                </plist>







