/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://gre/modules/WebappRT.jsm");

function parameterizeAppWindow(installRecord, manifest) {
  //now configure the page
  let topwindow = document.getElementById("topwindow");
  topwindow.setAttribute("title", manifest.name);

  //embed the actual content browser frame,
  //pointing at the origin+launch path
  let contentThing = document.createElement("browser");
  contentThing.setAttribute("id", "appContent");
  contentThing.setAttribute("type", "content");

  let url = installRecord.origin;
  if (manifest.launch_path)
    url += manifest.launch_path;
  contentThing.setAttribute("src", url);
  contentThing.setAttribute("flex", "1");
  topwindow.appendChild(contentThing);
}

let installRecord = DOMApplicationRegistry.getApp(Webapp.origin);
DOMApplicationRegistry.getManifestFor(Webapp.origin, function(manifest) {
  parameterizeAppWindow(installRecord, manifest);
});
