// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

const { once } = require('node:events');

exports.loadAndWait = async (window, load) => {
  const ready = Promise.race([
    once(window, 'ready-to-show'),
    once(window, 'receive-lynx-window-error').then(([, , code, message]) => {
      throw new Error(`Lynx load failed (${String(code)}): ${String(message)}`);
    }),
  ]);
  load();
  await ready;
};
