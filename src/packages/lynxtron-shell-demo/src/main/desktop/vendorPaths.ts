// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import path from 'path';

const bundleFileName = 'main.lynx.bundle';

export const LYNX_BUNDLE_PATH = __dirname.includes('app.asar')
  ? path.join(process.resourcesPath, 'resources', 'app', bundleFileName)
  : path.join(__dirname, bundleFileName);
