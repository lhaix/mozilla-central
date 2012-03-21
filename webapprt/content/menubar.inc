# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

<menubar id="main-menubar"
         onpopupshowing="if (event.target.parentNode.parentNode == this &amp;&amp;
                            !('@mozilla.org/widget/nativemenuservice;1' in Cc))
                           this.setAttribute('openedwithkey',
                                             event.target.parentNode.openedWithKey);"
         style="border:0px;padding:0px;margin:0px;-moz-appearance:none">

#ifndef XP_MACOSX
  <!-- On Mac, the Quit item gets moved to the Application menu by nsMenuBarX.
     - And right now it's the only item in the File menu.  So if we put it
     - into that menu on Mac, the File menu shows up empty on that OS.
     - To work around that problem, we put the item into the Edit menu on Mac
     - (from which nsMenuBarX still moves it properly), and we don't create
     - the File menu in the first place on that OS.
     -
     - XXX Remove this hack after adding the first item that always shows up
     - in the File menu on Mac. -->
  <menu id="file-menu" label="&fileMenu.label;"
        accesskey="&fileMenu.accesskey;">
    <menupopup id="menu_FilePopup">
      <menuitem id="menu_FileQuitItem"
#ifdef XP_WIN
                label="&quitApplicationCmdWin.label;"
                accesskey="&quitApplicationCmdWin.accesskey;"
#else
                label="&quitApplicationCmd.label;"
                accesskey="&quitApplicationCmd.accesskey;"
#endif
#ifdef XP_UNIX
                key="key_quitApplication"
#endif
                command="cmd_quitApplication"/>
    </menupopup>
  </menu>
#endif

  <menu id="edit-menu" label="&editMenu.label;"
        accesskey="&editMenu.accesskey;">
    <menupopup id="menu_EditPopup"
               onpopupshowing="updateEditUIVisibility()"
               onpopuphidden="updateEditUIVisibility()">

#ifdef XP_MACOSX
      <!-- See the comment on the File menu above for why this menuitem is here,
         - and note that JavaScript code dynamically updates its label,
         - since Mac apps include the name of the app in the Quit menuitem. -->
      <menuitem id="menu_FileQuitItem"
                label="&quitApplicationCmd.label;"
                key="key_quitApplication"
                command="cmd_quitApplication"/>
#endif

      <menuitem id="menu_undo"
                label="&undoCmd.label;"
                key="key_undo"
                accesskey="&undoCmd.accesskey;"
                command="cmd_undo"/>
      <menuitem id="menu_redo"
                label="&redoCmd.label;"
                key="key_redo"
                accesskey="&redoCmd.accesskey;"
                command="cmd_redo"/>
      <menuseparator/>
      <menuitem id="menu_cut"
                label="&cutCmd.label;"
                key="key_cut"
                accesskey="&cutCmd.accesskey;"
                command="cmd_cut"/>
      <menuitem id="menu_copy"
                label="&copyCmd.label;"
                key="key_copy"
                accesskey="&copyCmd.accesskey;"
                command="cmd_copy"/>
      <menuitem id="menu_paste"
                label="&pasteCmd.label;"
                key="key_paste"
                accesskey="&pasteCmd.accesskey;"
                command="cmd_paste"/>
      <menuitem id="menu_delete"
                label="&deleteCmd.label;"
                key="key_delete"
                accesskey="&deleteCmd.accesskey;"
                command="cmd_delete"/>
      <menuseparator/>
      <menuitem id="menu_selectAll"
                label="&selectAllCmd.label;"
                key="key_selectAll"
                accesskey="&selectAllCmd.accesskey;"
                command="cmd_selectAll"/>
    </menupopup>
  </menu>
</menubar>
