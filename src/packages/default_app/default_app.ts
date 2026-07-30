// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// @ts-nocheck

import { app, shell, LynxWindow } from 'lynxtron';

let mainWindow: LynxWindow | null = null;

// Links shown on the default screen. Only these URLs may be opened
// externally from the renderer.
const EXTERNAL_LINKS = new Set([
  'https://lynxjs.org/next/lynxtron',
  'https://github.com/lynx-family/lynxtron',
  'https://github.com/lynx-community/lynxtron-examples',
]);

async function createWindow() {
  await app.whenReady();
  const mainWindow = new LynxWindow({
    width: 1200,
    height: 800,
  });

  return mainWindow;
}

export const loadFile = async (appPath: string) => {
  mainWindow = await createWindow();
  mainWindow.on('-lynx-invoke', (event, methodName, params) => {
    if (methodName === 'open-external') {
      const url = typeof params?.url === 'string' ? params.url : '';
      if (EXTERNAL_LINKS.has(url)) {
        shell.openExternal(url);
      }
      event.sendReply({});
    }
  });
  mainWindow.show();
  mainWindow.loadFile(appPath, {
    globalProps: {
      versions: {
        lynxtron: process.versions.lynxtron,
        node: process.versions.node,
      },
      execPath: process.execPath,
    },
  });
};
