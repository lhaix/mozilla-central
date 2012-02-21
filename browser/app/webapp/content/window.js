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

XPCOMUtils.defineLazyGetter(this, "injector", function() {
  let injector = {};
  Cu.import("chrome://webapp/content/injector.js", injector);
  return injector;
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
      var topwindow = document.getElementById("topwindow");
      topwindow.setAttribute("title", installRecord.manifest.name);

      //embed the actual content browser frame,
      //pointing at the origin+launch path
      var contentThing = document.createElement("browser");
      contentThing.setAttribute("id", "appContent");
      contentThing.setAttribute("type", "content");
      contentThing.setAttribute("src", installRecord.launch_url);
      contentThing.setAttribute("flex", "1");
      topwindow.appendChild(contentThing);

      // These commands are here as an example of the types of commands
      // we could add in the future
      var mainCommandSet = document.createElement("commandset");

      var commandNewWindow = document.createElement("command");
      commandNewWindow.setAttribute("id", "new_window");
      commandNewWindow.setAttribute("oncommand", "newWindow()");
      mainCommandSet.appendChild(commandNewWindow);

      var commandAbout = document.createElement("command");
      commandAbout.setAttribute("id", "about");
      commandAbout.setAttribute("oncommand", "about()");
      mainCommandSet.appendChild(commandAbout);

      topwindow.insertBefore(mainCommandSet, windowjs);

      // These menu elements are here as an example of the types
      // of menus we could add in the future
      var mainMenuBar = document.createElement("menubar");

      var fileMenu = document.createElement("menu");
      fileMenu.setAttribute("label", "File");

      var fileMenuPopup = document.createElement("menupopup");

      var menuItemAbout = document.createElement("menuitem");
      menuItemAbout.setAttribute("label", "About " + installRecord.manifest.name);
      menuItemAbout.setAttribute("command", "about");
      fileMenuPopup.appendChild(menuItemAbout);

      var menuItemNewWindow = document.createElement("menuitem");
      menuItemNewWindow.setAttribute("label", "New Window");
      menuItemNewWindow.setAttribute("command", "new_window");
      fileMenuPopup.appendChild(menuItemNewWindow);

      fileMenu.appendChild(fileMenuPopup);
      mainMenuBar.appendChild(fileMenu);
      var windowjs = document.getElementById("windowjs");
      topwindow.insertBefore(mainMenuBar, windowjs);
    } catch(e) {
      dump("ERROR: Exception trying to set up app window:\n"
          + e + "\n");
    }
  };
}

function verifyReceiptGetInstallRecordCallback(cb) {
  return function(record) {
    if (!record) {
      cb({"error": "Invalid Receipt"});
      return;
    }

    let receipt = parseReceipt(record.install_data.receipt);
    if (!receipt) {
      cb({"error": "Invalid Receipt"});
      return;
    }

    // Two status "flags", one for verify XHR other for
    // BrowserID XHR
    // These two XHRs run in parallel, and the last one
    // to finish invokes cb()
    let assertion;
    let verifyStatus = false;
    let assertStatus = false;

    let verifyURL = receipt.verify;
    let verifyReq = new XMLHttpRequest();
    verifyReq.open('GET', verifyURL, true);
    verifyReq.onreadystatechange = verifyReqOnReadyStateChange();
    verifyReq.send(null);

    // Start BrowserID verification
    var options = {"silent": true,
                   "requiredEmail": receipt.user.value};
    checkNativeIdentityDaemon(contentWindowRef.location,
                              options, checkNativeIdentityDaemonCallback());
  };
}

function base64urldecode(arg) {
    var s = arg;
    s = s.replace(/-/g, '+'); // 62nd char of encoding
    s = s.replace(/_/g, '/'); // 63rd char of encoding
    switch (s.length % 4) // Pad with trailing '='s
    {
        case 0: break; // No pad chars in this case
        case 2: s += "=="; break; // Two pad chars
        case 3: s += "="; break; // One pad char
        default:
          throw new InputException("Illegal base64url string");
    }
    return window.atob(s); // Standard base64 decoder
}

function parseReceipt(rcptData) {
  // rcptData is a JWT.  We should use a JWT library.
  var data = rcptData.split(".");
  if (data.length != 3)
      return null;

  // convert base64url to base64
  var payload = base64urldecode(data[1]);
  var parsed = JSON.parse(payload);

  return parsed;
}

function verifyReqOnReadyStateChange() {
  return function (aEvt) {
    if (verifyReq.readyState == 4) {
        // FIXME: 404? Yeah, because that's what we get now
        // Hook up to real verification when it's done on AMO
        // and change this to 200 !!!
        verifyStatus = true;
        if (verifyReq.status == 404) {
            if (verifyOnly
             && typeof verifyOnly == "function") {
                verifyOnly(receipt);
            }
            if (verifyStatus && assertStatus)
                cb({"success": {"receipt": receipt,
                                "assertion": assertion}});
        } else {
            if (verifyStatus && assertStatus)
                cb({"error":
                      "Invalid Receipt: " + req.responseText});
        }
    }
  };
}

function assertReqOnReadyStateChange() {
  return function(aEvt) {
    if (assertReq.readyState == 4) {
        assertStatus = true;
        if (assertReq.status == 200) {
            if (verifyStatus && assertStatus)
                cb({"success": {"receipt": receipt,
                                "assertion": assertion}});
        } else {
            if (verifyStatus && assertStatus)
                cb({"error": "Invalid Identity: "
                            + assertReq.responseText});
        }
    }
  };
}

function checkNativeIdentityDaemonCallback() {
  return function(ast) {
    assertion = ast;
    if (!assertion) {
        cb({"error": "Invalid Identity"});
        return;
    }

    var assertReq = new XMLHttpRequest();
    assertReq.open('POST',
                   'https://browserid.org/verify',
                   true);
    assertReq.onreadystatechange = assertReqOnReadyStateChange();

    var body = "assertion="
             + encodeURIComponent(assertion)
             + "&audience="
             + contentWindowRef.location.protocol
             + "//"
             + contentWindowRef.location.host;
    assertReq.send(body);
  };
}

function verifyReceipt(options, cb, verifyOnly) {
  WebAppService.getInstallRecord(verifyReceiptGetInstallRecordCallback(cb));
}

function checkNativeIdentityDaemonOnTransportStatus() {
  return function(aTransport,
                  aStatus,
                  aProgress,
                  aProgressMax) {
    if (aStatus == aTransport.STATUS_CONNECTED_TO) {
        output.write(buf, buf.length);
    } else if (aStatus == aTransport.STATUS_RECEIVING_FROM) {
        var chunk = scriptableStream.read(8192);
        if (chunk.length> 1) {
            success(chunk);
            return;
        } else {
            failure();
            return;
        }
    } else if (false /* connection refused */) {
        port++;
        attemptConnection();
    }
  };
}

function checkNativeIdentityDaemon(callingLocation,
                                   options,
                                   success,
                                   failure)
{
  dump("INFO: CheckNativeIdentityDaemon "
      + callingLocation + " "
      + options + "\n");
  // XXX what do we do if we are not passed a requiredEmail?
  // could fail immediately, or could ask Firefox for a default somehow
  if (!options || !options.requiredEmail) {
    failure();
  }

  var port = 7350;
  var output, input, scriptableStream;
  var sockTransportService =
              Cc["@mozilla.org/network/socket-transport-service;1"]
              .getService(Ci.nsISocketTransportService);

  var domain = callingLocation.protocol + "//" + callingLocation.host;
  if (callingLocation.port && callingLocation.port.length > 0) {
    callingLocation += ":" + callingLocation.port;
  }
  var buf = "IDCHECK " + options.requiredEmail + " " + domain + "\r\n\r\n";

  var eventSink = {
      onTransportStatus: checkNativeIdentityDaemonOnTransportStatus(),
  };

  var threadMgr = Cc["@mozilla.org/thread-manager;1"].getService();
  function attemptConnection() {
      if (port > 7550) {
          failure();
          return;
      }
      var transport = sockTransportService.createTransport(null,
                                                           0,
                                                           "127.0.0.1",
                                                           port,
                                                           null);
      transport.setEventSink(eventSink, threadMgr.currentThread);
      try {
          output = transport.openOutputStream(transport.OPEN_BLOCKING, 0, 0);
          output.write(buf, buf.length);
          input = transport.openInputStream(transport.OPEN_BLOCKING, 0, 0);
          scriptableStream = Cc["@mozilla.org/scriptableinputstream;1"] 
                             .createInstance(Ci.nsIScriptableInputStream);
          scriptableStream.init(input);
      } catch (e) {
          alert(e);
          port++;
          attemptConnection();
      }
  }
  attemptConnection();
}

// XXX what is the options API going to be?
function getVerifiedEmail(callback, options) {
   checkNativeIdentityDaemon(
                 contentWindowRef.location,
                 options,
                 getVerifiedEmailCheckNativeIdentityDaemonOnSuccess(),
                 getVerifiedEmailCheckNativeIDentityDaemonOnFailure());
}

function getVerifiedEmailCheckNativeIdentityDaemonOnSuccess() {
  return function(assertion) {
    // success: return to caller
    var assert = JSON.parse(assertion);
    if (assert.status == "ok" && assert.assertion) {
        callback(assert.assertion);
    } else {
        // failure
        callback(null);
    }
  };
}

function getVerifiedEmailCheckNativeIdentityDaemonOnFailure() {
  return function() {
    // failure: need to present BrowserID dialog
    if (!options || !options.silent) {
        dump("INFO: OpenBrowserIDDialog\n");
        openBrowserIDDialog(callback, options);
    } else {
        callback(null);
    }
  };
}

function registerAppMenu(menuNode) {
  function createCommand(sourceNode) {
    var tag = sourceNode.tagName.toLowerCase();
    if (tag == "button") {
        var newMenuItem =
                window.document.createElementNS(XUL_NS,
                                                "menuitem");
        newMenuItem.setAttribute("label", sourceNode.textContent);

        // Apply key shortcuts
        var accessKey = sourceNode.getAttribute("accesskey");
        if (accessKey) {
            var newKey =
                  window.document.createElementNS(XUL_NS, "key");
            newKey.setAttribute("id", "key_" + accessKey);
            newKey.setAttribute("key", accessKey);
            newKey.setAttribute("modifiers", "meta");
            keySet.appendChild(newKey);
            newMenuItem.setAttribute("key", "key_" + accessKey);
        }
        newMenuItem.clickTarget = sourceNode;
        newMenuItem.setAttribute("oncommand",
                                 "event.target.clickTarget.click()"
                                );
        return newMenuItem;

    } else if (tag == "a") {

    } else if (tag == "option") {

    } else if (tag == "command") {

    } else if (tag == "label") {

    }
  }

  function addSeparator(targetPopup) {
      var separator =
              window.document.createElementNS(XUL_NS,
                                              "menuseparator");
      targetPopup.appendChild(separator);
  }

  // Applies
  // http://www.w3.org/TR/html5/interactive-elements.html
  // #the-menu-element
  // section 4.11.4.2 logic.
  function translateMenuElement(htmlElem, targetPopup) {
    if (htmlElem.tagName.toLowerCase() == "button") {
        var cmd = createCommand(htmlElem);
        targetPopup.appendChild(cmd);
    }
    else if (htmlElem.tagName.toLowerCase() == "hr" ) 
    {
        addSeparator(targetPopup);
    } else if (htmlElem.tagName.toLowerCase() == "option"
        && item.getAttribute("value")
        && item.getAttribute("value").length == 0
        && item.getAttribute("disabled"))
    {
        addSeparator(targetPopup);

    } else if (htmlElem.tagName.toLowerCase() == "li") {
        translateChildren(htmlElem, targetPopup);
    } else if (htmlElem.tagName.toLowerCase() == "label") {
        translateChildren(htmlElem, targetPopup);
    } else if (htmlElem.tagName.toLowerCase() == "menu") {
        if (htmlElem.getAttribute("label")) {
            // Append a submenu to the menu, using the value
            // of the element's label
            // attribute as the label of the menu.
            // The submenu must be constructed by
            // taking the element and creating a new menu
            // for it using the complete
            // process described in this section.

            var newMenu =
                  window.document.createElementNS(XUL_NS, "menu");
            var newMenuPopup =
                  window.document.createElementNS(XUL_NS,
                                                  "menupopup");
            newMenu.setAttribute("label",
                                 htmlElem.getAttribute("label"));
            newMenu.appendChild(newMenuPopup);
            targetPopup.appendChild(newMenu);

            var type = htmlElem.getAttribute("type");
            if (type && type.toLowerCase() == "help") {
                newMenu.id = "helpMenu";
            }
            translateChildren(htmlElem, newMenuPopup);

        } else {
            // Menu with no label: append separator and then
            // iterate, then another separator
            addSeparator(targetPopup);
            // iterate
            addSeparator(targetPopup);
        }
    } else if (htmlElem.tagName.toLowerCase() == "select") {
        // Select: append separator and then iterate, then
        // another separator
        addSeparator(targetPopup);
        // iterate
        addSeparator(targetPopup);

    } else if (htmlElem.tagName.toLowerCase() == "optgroup"
            && item.getAttribute("label")) {
        // Submenu...
    }
  }

  function translateChildren(parent, targetElem) {
      var item = parent.firstChild;
      while (item) {
          if (item.nodeType == Node.ELEMENT_NODE) {
              translateMenuElement(item, targetElem);
          }
          item = item.nextSibling;
      }
  }


  if (menuNode.tagName.toLowerCase() != 'menu') {
      alert("Got tagName "
          + menuNode.tagName
          + " - this is surprising");
      return;
  }
  menuNode.style.display = 'none';
  var mainMenu = window.document.getElementById("main_menu");
  var XUL_NS =
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  var commandSet = window.document.getElementById("main_commandset");
  var keySet = window.document.getElementById("main_keyset");

  // Based on
  // http://www.w3.org/TR/html5/commands.html#concept-command
  var cmdCounter = 1;

  // Remove existing menu items:
  while(mainMenu.firstChild) {
    mainMenu.removeChild(mainMenu.firstChild);
  }
  //while(commandSet.firstChild) {
  //  commandSet.removeChild(commandSet.firstChild);
  //}
  //
  // while(keySet.firstChild) {
  //  keySet.removeChild(keySet.firstChild);
  // }

  // And recurse to kick it all off:
  translateChildren(menuNode, mainMenu);
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

// Register the injector to add new APIs to the content windows:
try {
  injector.InjectorInit(window);
  // we inject on the XUL window and observe content-document-global-created
  // theBrowser.contentWindow.wrappedJSObject);
  // wrappedJSObject the right thing here?

  window.appinjector.register({
    apibase: "navigator.mozApps",
    name: "amInstalled",
    script: null,
    getapi: function() {
      return function(contentWindowRef) {
        return WebAppService.getInstallRecord;
      };
    }(),
  });

  window.appinjector.register({
    apibase: "navigator.mozApps",
    name: "verifyReceipt",
    script: null,
    getapi: function() {
      return function(contentWindowRef) {
        return verifyReceipt;
      };
    }(),
  });

  window.appinjector.register({
    apibase: "navigator.id",
    name: "getVerifiedEmail",
    script: null,
    getapi: function() {
      return function(contentWindowRef) {
        return getVerifiedEmail;
      };
    }(),
  });

  window.appinjector.register({
    apibase: "navigator.mozApps",
    name: "registerAppMenu",
    script: null,
    getapi: function() {
      return function(contentWindowRef) {
        return registerAppMenu;
      };
    }(),
  });

  window.appinjector.inject();
} catch (e) {
    dump("ERROR: Failure trying to inject mozapps API: " + e);
}
