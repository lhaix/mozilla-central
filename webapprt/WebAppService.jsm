/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// XXX Refactor with Webapps.jsm so there's only one API for accessing install
// records.

const EXPORTED_SYMBOLS = ["getInstallRecord"];

const Cc = Components.classes;
const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  Cu.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

XPCOMUtils.defineLazyGetter(this, "FileUtils", function() {
  Cu.import("resource://gre/modules/FileUtils.jsm");
  return FileUtils;
});

const configFile = FileUtils.getFile("AppRegD", ["config.json"]);

function getJSONFile(file, callback) {
  let channel = NetUtil.newChannel(file);
  channel.contentType = "application/json";
  NetUtil.asyncFetch(channel, function(stream, result) {
    try {
      if (!Components.isSuccessCode(result))
        throw("couldn't read from JSON file " + file.path + ": " + result);
      callback(JSON.parse(NetUtil.readInputStreamToString(stream,
                                                          stream.available())));
    }
    finally {
      stream.close();
    }
  });
}

let installRecord;
function getInstallRecord(callback) {
  dump("getting install record\n");

  if (installRecord) {
    dump("returning cached install record\n");

    // we use a timer here to make sure it is always async
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback({ "notify": function() callback(installRecord) },
                           0, timer.TYPE_ONE_SHOT);
    return;
  }

  getJSONFile(configFile, function(config) {
    dump("got config file: " + JSON.stringify(config) + "\n");
    let appsDir = Cc["@mozilla.org/file/local;1"].
                  createInstance(Ci.nsILocalFile);
    appsDir.initWithPath(config.profile);
    appsDir.append("webapps");
    let appsFile = appsDir.clone();
    appsFile.append("webapps.json");
    getJSONFile(appsFile, function(apps) {
      dump("got apps file: " + JSON.stringify(apps) + "\n");
      let appID;
      for (let id in apps) {
        if (apps[id].origin == config.origin) {
          appID = id;
          break;
        }
      }

      if (!appID)
        throw "app ID not found";

      let manifestFile = appsDir.clone();
      manifestFile.append(appID);
      manifestFile.append("manifest.json");
      getJSONFile(manifestFile, function(manifest) {
        dump("got manifest: " + JSON.stringify(manifest) + "\n");
        installRecord = apps[appID];
        installRecord.manifest = manifest;
        callback(installRecord);
      })
    });
  });
}
