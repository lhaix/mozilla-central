/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["Webapp"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "FileUtils", function() {
  Cu.import("resource://gre/modules/FileUtils.jsm");
  return FileUtils;
});

Object.defineProperty(this, "Webapp", {
  get: function getWebapp() {
    let configFile = FileUtils.getFile("AppRegD", ["config.json"]);
    let inputStream = Cc["@mozilla.org/network/file-input-stream;1"].
                      createInstance(Ci.nsIFileInputStream);
    inputStream.init(configFile, -1, 0, 0);
    let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
    let Webapp = json.decodeFromStream(inputStream, configFile.fileSize);
    delete this.Webapp;
    return this.Webapp = deepFreeze(Webapp);
  }
});

function deepFreeze(o) {
  Object.freeze(o); // First freeze the object.
  for (prop in o) {
    if (!o.hasOwnProperty(prop) || !(typeof o === "object") || Object.isFrozen(o)) {
      // If the object is on the prototype, not an object, or is already frozen,
      // skip it. Note that this might leave an unfrozen reference somewhere in the
      // object if there is an already frozen object containing an unfrozen object.
      continue;
    }

    deepFreeze(o[prop]); // Recursively call deepFreeze.
  }
  return o;
}
