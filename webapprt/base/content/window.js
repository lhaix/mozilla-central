const Cc = Components.classes;
const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "Services", function() {
  Cu.import("resource://gre/modules/Services.jsm");
  return Services;
});

XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  Cu.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

XPCOMUtils.defineLazyGetter(this, "FileUtils", function() {
  Cu.import("resource://gre/modules/FileUtils.jsm");
  return FileUtils;
});

XPCOMUtils.defineLazyGetter(this, "WebAppService", function() {
  let WebAppService = {};
  Cu.import("resource://gre/modules/WebAppService.jsm", WebAppService);
  return WebAppService;
});

function parameterizeAppWindow() {
  return function (installRecord) {
    try {
      //now configure the page
      let topwindow = document.getElementById("topwindow");
      topwindow.setAttribute("title", installRecord.manifest.name);

      //embed the actual content browser frame,
      //pointing at the origin+launch path
      let contentThing = document.createElement("browser");
      contentThing.setAttribute("id", "appContent");
      contentThing.setAttribute("type", "content");
      contentThing.setAttribute("src", installRecord.launch_url);
      contentThing.setAttribute("flex", "1");
      topwindow.appendChild(contentThing);

      // Hook up commands to be used by menu items
      let newWindowCommand = document.getElementById("new_window_command");
      // TODO: Implement the function newWindow() and uncomment this line
      //newWindowCommand.setAttribute("oncommand", "newWindow()");

      let quitCommand = document.getElementById("quit_command");
      // TODO: Implement the function quitApp() and uncomment this line
      //newWindowCommand.setAttribute("oncommand", "quitApp()");

      let aboutCommand = document.getElementById("about_command");
      // TODO: Implement the function about() and uncomment this line
      //aboutCommand.setAttribute("oncommand", "about()");

      let copyCommand = document.getElementById("copy_command");
      // TODO: Implement the function copy() and uncomment this line
      //copyCommand.setAttribute("oncommand", "copy()");

      let cutCommand = document.getElementById("cut_command");
      // TODO: Implement the function cut() and uncomment this line
      //cutCommand.setAttribute("oncommand", "cut()");

      let pasteCommand = document.getElementById("paste_command");
      // TODO: Implement the function paste() and uncomment this line
      //pasteCommand.setAttribute("oncommand", "paste()");

      // Parameterize individual menu items
      let menuItemAbout = document.getElementById("about_menu_item");
      menuItemAbout.setAttribute("label", "About "
                                        + installRecord.manifest.name);
      // TODO: When the `about` command  is implemented, uncomment this line
      //menuItemAbout.setAttribute("command", "about_command");
    } catch(e) {
      dump("ERROR: Exception trying to set up app window:\n"
          + e + "\n");
    }
  };
}

try {
  WebAppService.getInstallRecord(parameterizeAppWindow());
} catch(e) {
  dump("ERROR: Failed to get install record: " + e);
}

try {
  window.addEventListener("click", function(e) {
      // Make sure clicks remain in our context
      // TODO check to see if we are in same origin?
      if (e.target.nodeName == "A") {
          e.preventDefault();
          window.location = e.target.href;
      }
  }, false);
} catch(e) {
  dump("Error: Failed to register event listeners: " + e);
}
