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

In ${AppDir}, create an application.ini according to this template:
  [App]
  Name=Name of your app (e.g. Z-Type)
  Profile=Name of the app-specific directory you created

  [WebAppRT]
  FirefoxDir=Absolute path to dist/bin
