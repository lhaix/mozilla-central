#ifdef XP_MACOSX
// On Mac, we dynamically create the label for the Quit menuitem, using
// a string property to inject the name of the webapp into it.
window.addEventListener("load", function onLoadUpdateQuitMenuItem() {
  window.removeEventListener("load", onLoadUpdateQuitMenuItem, false);
  Components.utils.import("resource://gre/modules/Services.jsm");
  Components.utils.import("resource://gre/modules/WebappRT.jsm");
  Components.utils.import("resource://gre/modules/Webapps.jsm");
  let installRecord = DOMApplicationRegistry.getApp(Webapp.origin);
  let manifest = DOMApplicationRegistry.getManifestSync(Webapp.origin);
  let bundle =
    Services.strings.createBundle("chrome://webapprt/locale/window.properties");
  let quitLabel = bundle.formatStringFromName("quitApplicationCmdMac.label",
                                              [manifest.name], 1);
  document.getElementById("menu_FileQuitItem").setAttribute("label", quitLabel);
}, false);
#endif
