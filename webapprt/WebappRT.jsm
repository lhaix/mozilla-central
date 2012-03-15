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

// WebappRTDirectoryProvider needs to access the profile path synchronously,
// so we load the configuration file that way.
let Webapp = {
  get origin() {
    return this._config.origin;
  },

  get profile() {
    return this._config.profile;
  },

  get _config() {
    let configFile = FileUtils.getFile("AppRegD", ["config.json"]);
    let inputStream = Cc["@mozilla.org/network/file-input-stream;1"].
                      createInstance(Ci.nsIFileInputStream);
    inputStream.init(configFile, -1, 0, 0);
    let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
    let config = json.decodeFromStream(inputStream, configFile.fileSize);

    delete this._config;
    return this._config = config;
  }

};
