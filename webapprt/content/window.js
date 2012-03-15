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

      let url = installRecord.origin;
      if (installRecord.manifest.launch_path)
        url += installRecord.manifest.launch_path;
      contentThing.setAttribute("src", url);
      contentThing.setAttribute("flex", "1");
      topwindow.appendChild(contentThing);
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
