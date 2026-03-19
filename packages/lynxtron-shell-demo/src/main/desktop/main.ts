// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { app, LynxWindow, dialog } from '@lynx-js/lynxtron';
import { LYNX_BUNDLE_PATH } from './vendorPaths';
import path from 'path';
const isDev = process.env.NODE_ENV === 'development';

app.whenReady().then(() => {
  const w = new LynxWindow({
    width: 800,
    height: 600,
    title: 'Lynxtron Hello World',
    nodeIntegration: {
      preload_paths: [path.join(__dirname, 'preload.js')],
    },
  });

  // Handle bridge calls from Lynx UI
  // @ts-ignore
  w.on(
    '-lynx-invoke',
    async (callback: EventCallback, name: string, data: any) => {
      // In our architecture, UI calls NativeModules.bridge.request({ method, params })
      console.log(
        `[PC_Host] NativeModule Call: bridge.${name}`,
        data,
        callback,
        name
      );

      if (name === 'showDialog') {
        const { message } = data;
        dialog.showMessageBox({ message });
        callback.sendReply();
      } else if (name == 'getAppVersion') {
        callback.sendReply(app.getVersion());
      }
    }
  );

  w.show();
  if (isDev) {
    w.loadURL('http://localhost:5969/main.lynx.bundle');
  } else {
    w.loadFile(LYNX_BUNDLE_PATH);
  }
});
