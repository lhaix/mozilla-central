
let EXPORTED_SYMBOLS = ["getInstallRecord"];

Components.utils.import("resource://gre/modules/NetUtil.jsm");  
Components.utils.import("resource://gre/modules/FileUtils.jsm");  
Components.utils.import("resource://gre/modules/Services.jsm");

//globals
var origin;
var installRecord = undefined;

function getInstallRecord(callback) {

//get the install record. very important :)
var appPrefDir = Components.classes["@mozilla.org/file/directory_service;1"].  
                    getService(Components.interfaces.nsIProperties).  
                    get("AppRegD", Components.interfaces.nsIFile);  

//make the path to the config file
var configFile = Components.classes['@mozilla.org/file/local;1'].createInstance(Components.interfaces.nsILocalFile);
configFile.initWithFile(appPrefDir);
configFile.appendRelativePath("config.json");

//we use a timer here to make sure it is always async
if (installRecord) {
    dump("#######  using cached value of install record\n");
    var timer = Components.classes["@mozilla.org/timer;1"].createInstance(Components.interfaces.nsITimer);
    timer.initWithCallback({"notify": function() {
        dump("####### cached value callback\n");
        callback(installRecord);}}, 0, timer.TYPE_ONE_SHOT);
    return;
}

dump("#######  OPENING DATABASE to fetch install record. Ideally this should only be done once per app.\n");

//otherwise
//open the config file
NetUtil.asyncFetch(configFile, function(inputStream, status) {  
    if (!Components.isSuccessCode(status)) {  
        // Handle error!  
        dump("ERROR: " + status + " failed to read file: " + configFile);
        return;  
    }  
    //read the file
    var data = NetUtil.readInputStreamToString(inputStream, inputStream.available());  
    //parse the file
    var config = JSON.parse(data);
    inputStream.close();

    // dump("### ORIGIN:" + config.origin);
    // dump("### PROFILE:" + config.profile);

    //open the database and look for the installrecord
    //find the profiles directory of Firefox, not our current app
    var libDir = Components.classes["@mozilla.org/file/directory_service;1"].  
                    getService(Components.interfaces.nsIProperties).  
                    get("ULibDir", Components.interfaces.nsIFile); 

    var ffProfiles = Components.classes['@mozilla.org/file/local;1'].createInstance(Components.interfaces.nsILocalFile);
    //FIX FIX!!! MAC ONLY!!!!!!!!!               
    ffProfiles.initWithPath(libDir.path + "/Application Support/Firefox/Profiles");
    // dump("### FFPROFILES:   " + ffProfiles.path);

    //create the path to the app database, and open it
    var dbFile = Components.classes['@mozilla.org/file/local;1'].createInstance(Components.interfaces.nsILocalFile);
    dbFile.initWithFile(ffProfiles);
    dbFile.appendRelativePath(config.profile);
    dbFile.append("applications.sqlite");
    // dump("### DATABASE PATH: " + dbFile.path);

    let db = Services.storage.openUnsharedDatabase(dbFile);
    let stmt = db.createStatement("SELECT data FROM app WHERE key = '" + config.origin + "'");
    if (stmt.executeStep()) {
        var jdata = stmt.getString(0);
        installRecord = JSON.parse(jdata);
        callback(installRecord);
        // dump("### PARSED MANIFEST: " + JSON.stringify(installRecord));
    }
    stmt.finalize();

    });
}