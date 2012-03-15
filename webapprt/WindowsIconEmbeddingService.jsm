/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// TODO: TEST THIS FILE WITH A .ICO THAT HAS MULTIPLE ICONS IN IT!!!

const EXPORTED_SYMBOLS = ["embedIcon"];

const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;
const Ci = Components.interfaces;
const CC = Components.Constructor;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "ctypes", function() {
  Cu.import("resource://gre/modules/ctypes.jsm");
  return ctypes;
});

XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  Cu.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

// Source:
// http://msdn.microsoft.com/en-us/library/aa383751.aspx
const BYTE = ctypes.unsigned_char;
const WORD = ctypes.unsigned_short;
const LPWORD = WORD.ptr;
const DWORD = ctypes.unsigned_long;
const LPDWORD = DWORD.ptr;
const WCHAR = ctypes.jschar;
const LPWSTR = WCHAR.ptr;
const LPCWSTR = LPWSTR;
const HANDLE = ctypes.voidptr_t;
const LPVOID = ctypes.voidptr_t;
const BOOL = ctypes.int;
const ULONG_PTR = ctypes.uint64_t;

// Source
// http://msdn.microsoft.com/en-us/library/ms648009%28v=vs.85%29.aspx
const _RT_ICON = new ctypes.uint64_t(3);
const RT_ICON = ctypes.cast(_RT_ICON, LPWSTR);
const _RT_GROUP_ICON = new ctypes.uint64_t(14);
const RT_GROUP_ICON = ctypes.cast(_RT_GROUP_ICON, LPWSTR);

// Source:
// ???
const FALSE = new BOOL(0);
const TRUE = new BOOL(1);

const IconHeader = ctypes.StructType("IconHeader",
                                        [
                                          {"reserved": WORD},
                                          {"resourceType": WORD},
                                          {"imageCount": WORD},
                                        ]);

const IconDirEntry = ctypes.StructType("IconDirEntry",
                                        [
                                          {"width": BYTE},
                                          {"height": BYTE},
                                          {"colors": BYTE},
                                          {"reserved": BYTE},
                                          {"planes": WORD},
                                          {"bitsPerPixel": WORD},
                                          {"imageSizeBytes": DWORD},
                                          {"imageOffset": DWORD},
                                        ]);

const IconResEntry = ctypes.StructType("IconResEntry",
                                        [
                                          {"width": BYTE},
                                          {"height": BYTE},
                                          {"colors": BYTE},
                                          {"reserved": BYTE},
                                          {"planes": WORD},
                                          {"bitsPerPixel": WORD},
                                          {"imageSizeBytes": DWORD},
                                          {"resourceID": WORD},
                                        ]);

function onIconFileFetched(targetFile) {
  return function(iconFileStream, statusCode) {
    if (!Components.isSuccessCode(statusCode)) {
      Cu.reportError("Failed reading icon file, status=" + statusCode);
      return;
    }

    let kernel32;
    let EndUpdateResource;
    let hRes;
    let shouldRollBack = TRUE;

    try {
      let data = new CC("@mozilla.org/binaryinputstream;1",
                        "nsIBinaryInputStream",
                        "setInputStream")(iconFileStream)
                     .readByteArray(iconFileStream.available());

      let cDataArrayType = BYTE.array(data.length);
      let cData = new cDataArrayType();

      for(let curByte = 0; curByte < data.length; curByte++) {
        cData[curByte] = new BYTE(data[curByte]);
      }

      let iconHeader = ctypes.cast(cData.addressOfElement(0)
                                 , IconHeader.ptr).contents;

      Cu.reportError("ICONHEADER=" + iconHeader + "\n");

      if(0 !== iconHeader.reserved) {
        throw("'Reserved' is not set to 0!");
      }

      if(1 !== iconHeader.resourceType) {
        throw("'ResourceType' is not set to 1!");
      }

      if(iconHeader.imageCount < 1 || iconHeader.imageCount > 4) {
        throw("Unexpected number of icons in file, got: "
            + iconHeader.imageCount);
      }

      kernel32 = ctypes.open("Kernel32.dll");

      let BeginUpdateResource = kernel32.declare("BeginUpdateResourceW",
                                                 ctypes.winapi_abi,
                                                 HANDLE,
                                                 LPCWSTR,
                                                 BOOL);
      let UpdateResource = kernel32.declare("UpdateResourceW",
                                            ctypes.winapi_abi,
                                            BOOL,
                                            HANDLE,
                                            LPCWSTR,
                                            LPCWSTR,
                                            WORD,
                                            LPVOID,
                                            DWORD);
      EndUpdateResource = kernel32.declare("EndUpdateResourceW",
                                           ctypes.winapi_abi,
                                           BOOL,
                                           HANDLE,
                                           BOOL);

      // Create the "group" struct.  This needs to be embedded into the file
      // in addition to each icon.
      let groupArrayType = BYTE.array(IconHeader.size
                                    + (IconResEntry.size
                                       * iconHeader.imageCount));
      let group = new groupArrayType();
      // The group struct has the same header as the .ico file.
      for(let curByte = 0; curByte < IconHeader.size; curByte++) {
        group[curByte] = new BYTE(data[curByte]);
      }

      hRes = BeginUpdateResource(targetFile.path, FALSE);
      if(null === hRes) {
        throw("Failure in BeginUpdateResource");
      }

      for(let imageIndex = 0;
          imageIndex < iconHeader.imageCount;
          imageIndex++) {
        let byteIndexDir = IconHeader.size
                         + (imageIndex * IconDirEntry.size);

        let curIconDirEntry = ctypes.cast(cData.addressOfElement(byteIndexDir)
                                        , IconDirEntry.ptr).contents;

        let resourceID = ctypes.cast(new ctypes.uint64_t(imageIndex + 1), LPWSTR);

        // Call UpdateResource for each icon (embed image data as resources)
        if(FALSE === UpdateResource(hRes,
                                    RT_ICON,
                                    resourceID,
                                    0, // MAKELANGID(LANG_NEUTRAL,
                                       //            SUBLANG_NEUTRAL)
                                    cData.addressOfElement(
                                              curIconDirEntry.imageOffset),
                                    curIconDirEntry.imageSizeBytes)) {
          throw("UpdateResource returned FALSE");
        }

        let byteIndexRes = IconHeader.size
                         + (imageIndex * IconResEntry.size);
        for(let curByte = 0; curByte < IconResEntry.size; curByte++) {
          group[byteIndexRes+curByte] = new BYTE(data[byteIndexDir+curByte]);
        }

        let curIconResEntry = ctypes.cast(group.addressOfElement(byteIndexRes)
                                        , IconResEntry.ptr).contents;
        curIconResEntry.resourceID = new WORD(imageIndex + 1);
      }

      // Call UpdateResource for the RT_GROUP_ICON (embed icon struct)
      if(FALSE === UpdateResource(hRes,
                                  RT_GROUP_ICON,
                                  "MAINICON",
                                  0, // MAKELANGID(LANG_NEUTRAL,
                                     //            SUBLANG_NEUTRAL)
                                  group.addressOfElement(0),
                                  group.length)) {
        throw("UpdateResource returned FALSE for group");
      }

      shouldRollBack = FALSE;
    } catch(e) {
      Cu.reportError("Failed updating icon resource: " + e);
      return;
    } finally {
      if(EndUpdateResource && hRes) {
        try {
          if(FALSE === EndUpdateResource(hRes, shouldRollBack)) {
            Cu.reportError("EndUpdateResource returned FALSE");
          }
        } catch(e) {
          Cu.reportError("Failure calling EndUpdateResource: " + e);
        }
      }

      if(kernel32)
      {
        try {
          kernel32.close();
        } catch(e) {

        }
      }

      try {
        iconFileStream.close();
      } catch(e) {
        Cu.reportError("Failure closing icon stream: " + e);
      }
    }
  }
}


function embedIcon(sourceFile,
                   targetFile) {
  if(!sourceFile
  || !sourceFile.exists()
  || !targetFile
  || !targetFile.exists()) {
    throw Cr.NS_ERROR_INVALID_ARG;
  }

  NetUtil.asyncFetch(sourceFile, onIconFileFetched(targetFile));
};
