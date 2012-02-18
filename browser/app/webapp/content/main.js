var os = Components.classes["@mozilla.org/xre/app-info;1"]
                   .getService(Components.interfaces.nsIXULRuntime).OS;
    
//only make the tiny window that handles window closing events (and quits the app) for Mac OS
if("Darwin" === os) {
    var observer = {
        observe: function(contentWindow, aTopic, aData) {
            if (aTopic == 'xul-window-destroyed') {
                // If there is nothing left but the main (invisible) window, quit
                var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]  
                           .getService(Components.interfaces.nsIWindowMediator);  
                var enumerator = wm.getEnumerator("app");
                if(enumerator.hasMoreElements()) return;

                var appStartup = Components.classes["@mozilla.org/toolkit/app-startup;1"].getService(Components.interfaces.nsIAppStartup);
                appStartup.quit(appStartup.eAttemptQuit);
            }
        }
    }

    // Register our observer:
    var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
    observerService.addObserver(observer, "xul-window-destroyed", false);
};
