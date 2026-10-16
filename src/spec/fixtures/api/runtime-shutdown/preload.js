// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

const { contextBridge } = require('lynxtron');
const fs = require('node:fs');

contextBridge.exposeInLynxBTS({
  mark(phase, id) {
    fs.appendFileSync(
      process.env.LYNXTRON_SHUTDOWN_MARKERS,
      `${phase}:${id}\n`
    );
  },
});
