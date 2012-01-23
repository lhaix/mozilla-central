function doHandleMenuBar(toCall)
{
    // We need to pageMod in from the faker
    // TODO pass this into content code somehow
    window.alert("Menu bar item " + toCall + " was clicked!");
    return;
}

window.addEventListener("click", function(e) {
    // Make sure clicks remain in our context
    // TODO check to see if we are in same origin?
    if (e.target.nodeName == "A") {
        e.preventDefault();
        window.location = e.target.href;
    }
}, false);

// Commands:
var appName = "Roundball";
function newWindow()
{
    var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                       .getService(Components.interfaces.nsIWindowWatcher);
    var win = ww.openWindow(null, "chrome://webapp/content/window.xul",
                            null, "chrome,centerscreen,resizable", null);
}


// Register the injector to add new APIs to the content windows:
try {
  var injector = {};
  Components.utils.import("chrome://webapp/content/injector.js", injector);
  
  function getInstallRecord(cb) {
    var appDirectory = Components.classes["@mozilla.org/file/directory_service;1"].  
                        getService(Components.interfaces.nsIProperties).  
                        get("CurProcD", Components.interfaces.nsIFile);  
              
    var aNsLocalFile = Components.classes['@mozilla.org/file/local;1'].createInstance(Components.interfaces.nsILocalFile);
    aNsLocalFile.initWithFile(appDirectory);
    aNsLocalFile.appendRelativePath("installrecord.json");

    Components.utils.import("resource://gre/modules/NetUtil.jsm");  
    Components.utils.import("resource://gre/modules/FileUtils.jsm");  
            
    dump(aNsLocalFile.path + "\n");
    NetUtil.asyncFetch(aNsLocalFile, function(inputStream, status) {  
        if (!Components.isSuccessCode(status)) {  
            // Handle error!  
            console.log("ERROR: " + status + " failed to read file: " + inFile);
            return;  
        }  
        var data = NetUtil.readInputStreamToString(inputStream, inputStream.available());  
        var parsed = JSON.parse(data);
        inputStream.close();
        cb(parsed);
    });
  }

  injector.InjectorInit(window); // we inject on the XUL window and observe content-document-global-created //   theBrowser.contentWindow.wrappedJSObject); // wrappedJSObject the right thing here?
  window.appinjector.register({
    apibase: "navigator.mozApps",
    name: "amInstalled",
    script: null,
    getapi: function(contentWindowRef) {
      return function(callback) {
        getInstallRecord(callback);
      }
    }
  });

  window.appinjector.register({
    apibase: "navigator.mozApps",
    name: "verifyReceipt",
    script: null,
    getapi: function(contentWindowRef) {
        return function(options, cb, verifyOnly) {
            getInstallRecord(function(record) {
                if (!record) {
                    cb({"error": "Invalid Receipt"});
                    return;
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
                        default: throw new InputException("Illegal base64url string!");
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
                
                var receipt = parseReceipt(record.install_data.receipt);
                if (!receipt) {
                    cb({"error": "Invalid Receipt"});
                    return;
                }
                
                // Two status "flags", one for verify XHR other for BrowserID XHR
                // These two XHRs run in parallel, and the last one to finish invokes cb()
                var assertion;
                var verifyStatus = false;
                var assertStatus = false;

                var verifyURL = receipt.verify;
                var verifyReq = new XMLHttpRequest();  
                verifyReq.open('GET', verifyURL, true);  
                verifyReq.onreadystatechange = function (aEvt) {  
                    if (verifyReq.readyState == 4) {
                        // FIXME: 404? Yeah, because that's what we get now
                        // Hook up to real verification when it's done on AMO
                        // and change this to 200 !!!
                        verifyStatus = true;
                        if (verifyReq.status == 404) {
                            if (verifyOnly && typeof verifyOnly == "function") {
                                verifyOnly(receipt);
                            }
                            if (verifyStatus && assertStatus)
                                cb({"success": {"receipt": receipt, "assertion": assertion}});
                        } else {
                            if (verifyStatus && assertStatus)
                                cb({"error": "Invalid Receipt: " + req.responseText});
                        }
                    }
                };
                verifyReq.send(null);

                // Start BrowserID verification
                var options = {"silent": true, "requiredEmail": receipt.user.value};
                checkNativeIdentityDaemon(contentWindowRef.location, options, function(ast) {
                    assertion = ast;
                    if (!assertion) {
                        cb({"error": "Invalid Identity"});
                        return;
                    }

                    var assertReq = new XMLHttpRequest();
                    assertReq.open('POST', 'https://browserid.org/verify', true);
                    assertReq.onreadystatechange = function(aEvt) {
                        if (assertReq.readyState == 4) {
                            assertStatus = true;
                            if (assertReq.status == 200) {
                                if (verifyStatus && assertStatus)
                                    cb({"success": {"receipt": receipt, "assertion": assertion}});
                            } else {
                                if (verifyStatus && assertStatus)
                                    cb({"error": "Invalid Identity: " + assertReq.responseText});
                            }
                        }
                    };

                    var body = "assertion=" + encodeURIComponent(assertion) + "&audience=" +
                        contentWindowRef.location.protocol + "//" + contentWindowRef.location.host;
                    assertReq.send(body);
                });
            });
        }
    }  
  });

  function checkNativeIdentityDaemon(callingLocation, options, success, failure)
  {
    dump("CheckNativeIdentityDaemon " + callingLocation + " " + options + "\n");
    // XXX what do we do if we are not passed a requiredEmail?
    // could fail immediately, or could ask Firefox for a default somehow
    if (!options || !options.requiredEmail) failure();

    var port = 7350;
    var output, input, scriptableStream;
    var sockTransportService = Components.classes["@mozilla.org/network/socket-transport-service;1"]
        .getService(Components.interfaces.nsISocketTransportService);
    
    var domain = callingLocation.protocol + "//" + callingLocation.host;
    if (callingLocation.port && callingLocation.port.length > 0) callingLocation += ":" + callingLocation.port;
    var buf = "IDCHECK " + options.requiredEmail + " " + domain + "\r\n\r\n";

    var eventSink = {
        onTransportStatus: function(aTransport, aStatus, aProgress, aProgressMax) {
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
        }
    };

    var threadMgr = Components.classes["@mozilla.org/thread-manager;1"].getService();
    function attemptConnection() {
        if (port > 7550) {
            failure();
            return;
        }
        var transport = sockTransportService.createTransport(null, 0, "127.0.0.1", port, null);
        transport.setEventSink(eventSink, threadMgr.currentThread);
        try {
            output = transport.openOutputStream(transport.OPEN_BLOCKING, 0, 0);
            output.write(buf, buf.length);
            input = transport.openInputStream(transport.OPEN_BLOCKING, 0, 0);
            scriptableStream = Components.classes["@mozilla.org/scriptableinputstream;1"]  
                                 .createInstance(Components.interfaces.nsIScriptableInputStream);  
            scriptableStream.init(input);
        } catch (e) {
            alert(e);
            port++;
            attemptConnection();
        }
    }
    attemptConnection();
  }

  window.appinjector.register({
    apibase: "navigator.id",
    name: "getVerifiedEmail",
    script: null,
    getapi: function(contentWindowRef) {
        return function(callback, options) { // XXX what is the options API going to be?
            checkNativeIdentityDaemon(contentWindowRef.location, options, function(assertion) {
                // success: return to caller
                var assert = JSON.parse(assertion);
                if (assert.status == "ok" && assert.assertion) {
                    callback(assert.assertion);
                } else {
                    // failure
                    callback(null);
                }
            }, function() {
                // failure: need to present BrowserID dialog
                if (!options || !options.silent) {
                    dump("OpenBrowserIDDialog\n");
                    openBrowserIDDialog(callback, options);
                } else {
                    callback(null);
                }
            });
        }
    }
  });

  window.appinjector.register({
    apibase: "navigator.mozApps",
    name: "newWindow",
    script: null,
    getapi: function(contentWindowRef) {
        return function() {
            newWindow();
        }
    }
  });

  window.appinjector.register({
    apibase: "navigator.mozApps",
    name: "registerAppMenu",
    script: null,
    getapi: function(contentWindowRef) {
        return function(menuNode) {
            if (menuNode.tagName.toLowerCase() != 'menu') {
                alert("Got tagName " + menuNode.tagName + " - this is surprising");
                return;
            }
            menuNode.style.display = 'none';
            var mainMenu = window.document.getElementById("main_menu");
            var XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
            var commandSet = window.document.getElementById("main_commandset");
            var keySet = window.document.getElementById("main_keyset");

            // Based on http://www.w3.org/TR/html5/commands.html#concept-command
            var cmdCounter = 1;
            function createCommand(sourceNode) {
                var tag = sourceNode.tagName.toLowerCase();
                if (tag == "button") {
                    var newMenuItem = window.document.createElementNS(XUL_NS, "menuitem");
                    newMenuItem.setAttribute("label", sourceNode.textContent);

                    // Apply key shortcuts
                    var accessKey = sourceNode.getAttribute("accesskey");
                    if (accessKey) {
                        var newKey = window.document.createElementNS(XUL_NS, "key");
                        newKey.setAttribute("id", "key_" + accessKey);
                        newKey.setAttribute("key", accessKey);
                        newKey.setAttribute("modifiers", "meta");
                        keySet.appendChild(newKey);
                        newMenuItem.setAttribute("key", "key_" + accessKey);
                    }
                    newMenuItem.clickTarget = sourceNode;
                    newMenuItem.setAttribute("oncommand", "event.target.clickTarget.click()");
                    return newMenuItem;

                } else if (tag == "a") {
                    
                } else if (tag == "option") {
                    
                } else if (tag == "command") {
                    
                } else if (tag == "label") {
                    
                }

            }

            function addSeparator(targetPopup) {
                var separator = window.document.createElementNS(XUL_NS, "menuseparator");
                targetPopup.appendChild(separator);            
            }

            // Applies http://www.w3.org/TR/html5/interactive-elements.html#the-menu-element
            // section 4.11.4.2 logic.
            function translateMenuElement(htmlElem, targetPopup) {

                if (htmlElem.tagName.toLowerCase() == "button") {
                    var cmd = createCommand(htmlElem);
                    targetPopup.appendChild(cmd);
                }
                else if (htmlElem.tagName.toLowerCase() == "hr" ) 
                {
                    addSeparator(targetPopup);
                } else if (htmlElem.tagName.toLowerCase() == "option" &&
                    item.getAttribute("value") && item.getAttribute("value").length == 0 &&
                    item.getAttribute("disabled"))
                {
                    addSeparator(targetPopup);
           
                } else if (htmlElem.tagName.toLowerCase() == "li") {
                    translateChildren(htmlElem, targetPopup);
                } else if (htmlElem.tagName.toLowerCase() == "label") {
                    translateChildren(htmlElem, targetPopup);
                } else if (htmlElem.tagName.toLowerCase() == "menu") {
                    if (htmlElem.getAttribute("label")) {
                        // Append a submenu to the menu, using the value of the element's label 
                        // attribute as the label of the menu. The submenu must be constructed by
                        // taking the element and creating a new menu for it using the complete
                        // process described in this section.

                        var newMenu = window.document.createElementNS(XUL_NS, "menu");
                        var newMenuPopup = window.document.createElementNS(XUL_NS, "menupopup");
                        newMenu.setAttribute("label", htmlElem.getAttribute("label"));
                        newMenu.appendChild(newMenuPopup);
                        targetPopup.appendChild(newMenu);
                        
                        var type = htmlElem.getAttribute("type");
                        if (type && type.toLowerCase() == "help") {
                            newMenu.id = "helpMenu";
                        }
                        translateChildren(htmlElem, newMenuPopup);

                    } else {
                        // Menu with no label: append separator and then iterate, then another separator
                        addSeparator(targetPopup);
                        // iterate
                        addSeparator(targetPopup);
                    }
                } else if (htmlElem.tagName.toLowerCase() == "select") {
                    // Select: append separator and then iterate, then another separator
                    addSeparator(targetPopup);
                    // iterate
                    addSeparator(targetPopup);
                                                    
                } else if (htmlElem.tagName.toLowerCase() == "optgroup" && item.getAttribute("label")) {
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
            // Remove existing menu items:
            while(mainMenu.firstChild) mainMenu.removeChild(mainMenu.firstChild);
            //while(commandSet.firstChild) commandSet.removeChild(commandSet.firstChild);
           // while (keySet.firstChild) keySet.removeChild(keySet.firstChild);
            
            // And recurse to kick it all off:
            translateChildren(menuNode, mainMenu);
        }
    }
  });

  window.appinjector.inject();
} catch (e) {
    dump(e + "\n");
}
