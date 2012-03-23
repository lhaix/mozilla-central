/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// On Mac, apps outlive their last window closing by default, but we want
// webapps to close when that happens, since there is no model on the web
// for apps remaining open when you close their window.  A simple, reliable
// way to do this is to call nsIAppStartup::exitLastWindowClosingSurvivalArea
// to undo the call to nsIAppStartup::enterLastWindowClosingSurvivalArea
// that makes apps outlive their last window closing.
Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup).
                                         exitLastWindowClosingSurvivalArea();
