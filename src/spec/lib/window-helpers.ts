// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { BaseWindow } from 'lynxtron';

import { expect } from 'chai';
import { once } from 'node:events';
import { setTimeout as delay } from 'node:timers/promises';

async function ensureWindowIsClosed(window: BaseWindow | null) {
  if (window && !window.isDestroyed()) {
    const closed = once(window, 'closed');
    window.destroy();
    await closed;
  }
}

export const closeWindow = async (
  window: BaseWindow | null = null,
  { assertNotWindows } = { assertNotWindows: true }
) => {
  await ensureWindowIsClosed(window);

  if (assertNotWindows) {
    let windows = BaseWindow.getAllWindows() as BaseWindow[];
    if (windows.length > 0) {
      // Wait until next tick to assert that all windows have been closed.
      await delay(0);
      windows = BaseWindow.getAllWindows() as BaseWindow[];
      try {
        expect(windows).to.have.lengthOf(0);
      } finally {
        for (const win of windows) {
          await ensureWindowIsClosed(win);
        }
      }
    }
  }
};

export async function closeAllWindows(assertNotWindows = false) {
  let windowsClosed = 0;
  for (const w of BaseWindow.getAllWindows()) {
    await closeWindow(w as BaseWindow, { assertNotWindows });
    windowsClosed++;
  }
  return windowsClosed;
}
