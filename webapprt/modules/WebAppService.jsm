const EXPORTED_SYMBOLS = ["getInstallRecord"];

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

const configFile = FileUtils.getFile("AppRegD", ["config.json"]);
var installRecord = null;

function onConfigFileFetched(callback) {
  return function(inputStream, status) {
    if (!Components.isSuccessCode(status)) {
        // Handle error!
        dump("ERROR: " + status + ". "
           + "Failed to read file: " + configFile
           + ".\n");
        return;
    }

    let db;
    let stmt;
    try {
      let data = NetUtil.readInputStreamToString(inputStream,
                                                 inputStream.available());
      let config = JSON.parse(data);

      //open the database and look for the install record
      let dbFile = Cc["@mozilla.org/file/local;1"]
                   .createInstance(Ci.nsILocalFile);
      dbFile.initWithPath(config.profile);
      dbFile.append("applications.sqlite");

      db = Services.storage.openUnsharedDatabase(dbFile);

      stmt = db.createStatement("SELECT data "
                              + "FROM app "
                              + "WHERE key = '" + config.origin + "'");

      if (!stmt.executeStep()) {
        dump("ERROR: App install record was not found. "
           + "Was this app uninstalled?\n");
        return;
      }

      installRecord = JSON.parse(stmt.getString(0));
    } catch(e) {
      dump("ERROR: Exception while trying to obtain install record:\n"
          + e + "\n");
      return;
    } finally {
      try{ inputStream.close(); } catch(e) { }
      try{ stmt.finalize(); } catch(e) { }
      try{ db.close(); } catch(e) { }
    }

    try {
      callback(installRecord);
    } catch(e) {
      throw("Exception in callback passed to getInstallRecord: " + e);
    }
  };
}

function invokeCallback(callback) {
  return function() {
    if(callback) {
      callback(installRecord);
    }
  };
}

function getInstallRecord(callback) {
  if (installRecord) {
    dump("Getting install record (cached)");
    //we use a timer here to make sure it is always async
    var timer = Cc["@mozilla.org/timer;1"]
                .createInstance(Ci.nsITimer);
    timer.initWithCallback({"notify": invokeCallback(callback)},
                           0,
                           timer.TYPE_ONE_SHOT);
    return;
  }

  dump("Getting install record");
  NetUtil.asyncFetch(configFile, onConfigFileFetched(callback));
}
