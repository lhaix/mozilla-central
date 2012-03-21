# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

<stringbundleset id="stringbundleset">
  <stringbundle id="bundle_brand" src="chrome://branding/locale/brand.properties"/>
  <stringbundle id="bundle_shell" src="chrome://browser/locale/shellservice.properties"/>
  <stringbundle id="bundle_preferences" src="chrome://browser/locale/preferences/preferences.properties"/>
</stringbundleset>

<commandset id="mainCommandSet">
  <command id="cmd_close" oncommand="BrowserCloseTabOrWindow()"/>
  <command id="cmd_quitApplication" oncommand="goQuitApplication()"/>
  <commandset id="editMenuCommands"/>
  <command id="View:FullScreen" oncommand="BrowserFullScreen();"/>
</commandset>

<keyset id="mainKeyset">
  <key id="key_closeWindow"
       key="&closeCmd.key;"
       command="cmd_closeWindow"
       modifiers="accel,shift"/>
  <key id="key_undo"
       key="&undoCmd.key;"
       modifiers="accel"/>
  <key id="key_redo" key="&undoCmd.key;" modifiers="accel,shift"/>
  <key id="key_cut"
       key="&cutCmd.key;"
       modifiers="accel"/>
  <key id="key_copy"
       key="&copyCmd.key;"
       modifiers="accel"/>
  <key id="key_paste"
       key="&pasteCmd.key;"
       modifiers="accel"/>
  <key id="key_delete" keycode="VK_DELETE" command="cmd_delete"/>
  <key id="key_selectAll" key="&selectAllCmd.key;" modifiers="accel"/>
  <key id="key_fullScreen"
       key="&fullScreenCmd.macCommandKey;"
       command="View:FullScreen"
       modifiers="accel,shift"/>
  <key keycode="VK_F11" command="View:FullScreen"/>
  <key id="key_close"
       key="&closeCmd.key;"
       command="cmd_close"
       modifiers="accel"/>
  <key id="key_quitApplication"
       key="&quitApplicationCmdMac.key;"
       command="cmd_quitApplication"
       modifiers="accel"/>
</keyset>

<commandset id="baseMenuCommandSet"/>

<keyset id="baseMenuKeyset"/>

