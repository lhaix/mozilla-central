/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const WEBAPPRT_DATA_DIR = "WebappRTDataD";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/WebappRT.jsm");

function DirectoryProvider() {}

DirectoryProvider.prototype = {
  classID: Components.ID("{e1799fda-4b2f-4457-b671-e0641d95698d}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider]),

  getFile: function(prop, persistent) {
    if (prop == WEBAPPRT_DATA_DIR) {
      let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      dir.initWithPath(WebappRT.webapp.profile);
      return dir;
    }

    // We return null to show failure instead of throwing an error,
    // which works with the way the interface is called (per bug 529077).
    return null;
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([DirectoryProvider]);
