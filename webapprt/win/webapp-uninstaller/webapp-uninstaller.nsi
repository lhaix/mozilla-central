# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is an NSIS uninstaller for Open Web Apps
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Tim Abraldes <tabraldes@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

!include "FileFunc.nsh"

SetCompressor /SOLID /FINAL lzma
CRCCheck on
RequestExecutionLevel user

SilentInstall silent

Var PARAMETERS

Var ORIGIN_SCHEME
Var ORIGIN_HOST
Var ORIGIN_PORT

Var APP_FILENAME

Name "Webapp Uninstaller Creator"
OutFile webapp-uninstaller-creator.exe

Function .onInit
FunctionEnd

Section Install
  WriteUninstaller $EXEDIR\webapp-uninstaller.exe
SectionEnd

Function un.onInit
FunctionEnd

Section un.Install
  ${GetParameters} $PARAMETERS
  IfErrors 0 +2
    Abort "Please use the Windows Control Panel to remove this application"
  ${GetOptions} $PARAMETERS "/ORIGIN_SCHEME= " $ORIGIN_SCHEME
  IfErrors 0 +2
    Abort "Please use the Windows Control Panel to remove this application"
  ${GetOptions} $PARAMETERS "/ORIGIN_HOST= " $ORIGIN_HOST
  IfErrors 0 +2
    Abort "Please use the Windows Control Panel to remove this application"
  ${GetOptions} $PARAMETERS "/ORIGIN_PORT= " $ORIGIN_PORT
  IfErrors 0 +2
    Abort "Please use the Windows Control Panel to remove this application"

  ReadRegStr $INSTDIR \
             HKCU \
            "Software\Microsoft\Windows\CurrentVersion\Uninstall\$ORIGIN_SCHEME://$ORIGIN_HOST:$ORIGIN_PORT" \
            "InstallLocation"
  IfErrors 0 +2
    Abort "The installation appears to be corrupted; cannot continue with uninstall"

  ReadRegStr $APP_FILENAME \
             HKCU \
            "Software\Microsoft\Windows\CurrentVersion\Uninstall\$ORIGIN_SCHEME://$ORIGIN_HOST:$ORIGIN_PORT" \
            "AppFilename"

  # Remove shortcuts
  Delete $SMPROGRAMS\$APP_FILENAME.lnk
  Delete $DESKTOP\$APP_FILENAME.lnk

  # Remove chrome dir
  Delete $INSTDIR\chrome\icons\default\topwindow.ico
  RMDIR $INSTDIR\chrome\icons\default
  RMDIR $INSTDIR\chrome\icons
  RMDIR $INSTDIR\chrome

  # Remove bin dir
  Delete $INSTDIR\bin\application.ini
  Delete $INSTDIR\bin\uninstall.exe
  Delete $INSTDIR\bin\$APP_FILENAME.exe
  Delete $INSTDIR\bin\webapprt.old
  RMDIR $INSTDIR\bin

  # TODO: If the user has said "please delete my data too,"
  #       RMDIR /r $INSTDIR\Profiles
  #       Delete $INSTDIR\profiles.ini
  #       Remove the equivalent data from AppData\Local

  # Remove toplevel items
  Delete $INSTDIR\config.json

  # Remove registry key (and values)
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$ORIGIN_SCHEME://$ORIGIN_HOST:$ORIGIN_PORT"
SectionEnd
