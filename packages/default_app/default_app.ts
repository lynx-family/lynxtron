// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// @ts-nocheck

import { app, LynxWindow } from 'lynxtron';
import * as path from 'node:path';
import * as url from 'node:url';

let mainWindow: LynxWindow | null = null;

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
  mainWindow.show();
  mainWindow.loadFile(appPath);
};
