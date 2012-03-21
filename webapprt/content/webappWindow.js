/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function onLoad() {
  window.removeEventListener("load", onLoad, false);

  let installRecord = DOMApplicationRegistry.getApp(Webapp.origin);
  let manifest = DOMApplicationRegistry.getManifestSync(Webapp.origin);

  // Set the title of the window to the name of the webapp.
  // XXX Set it to the webapp page's title, then update it when that changes.
  document.documentElement.setAttribute("title", manifest.name);

  // Load the webapp's launch path.
  let url = installRecord.origin;
  if (manifest.launch_path)
    url += manifest.launch_path;
  document.getElementById("content").setAttribute("src", url);
}
window.addEventListener("load", onLoad, false);
