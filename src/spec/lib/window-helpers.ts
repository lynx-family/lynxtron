// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { BaseWindow } from 'lynxtron';

import { expect } from 'chai';
import { once } from 'node:events';
import { setTimeout as delay } from 'node:timers/promises';

import { waitUntil } from './spec-helpers';

async function ensureWindowIsClosed(window: BaseWindow | null) {
  if (window && !window.isDestroyed()) {
    const closed = once(window, 'closed').catch(() => undefined);
    window.destroy();

    await Promise.race([
      closed,
      waitUntil(
        () =>
          window.isDestroyed() ||
          !(BaseWindow.getAllWindows() as BaseWindow[]).includes(window),
        { timeout: 3000 }
      ),
    ]).catch(() => undefined);
  }

  if (window) {
    await waitUntil(
      () => !(BaseWindow.getAllWindows() as BaseWindow[]).includes(window),
      { timeout: 3000 }
    ).catch(() => undefined);
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
      for (const win of windows) {
        await ensureWindowIsClosed(win);
      }

      // Wait until next tick so delayed native teardown can flush.
      await delay(0);
      await waitUntil(
        () => (BaseWindow.getAllWindows() as BaseWindow[]).length === 0,
        {
          timeout: 3000,
        }
      ).catch(() => undefined);

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
