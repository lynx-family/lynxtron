// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { defineProperties } from '@lynxtron/internal/common/define-properties';

const btsModuleList: LynxtronInternal.ModuleEntry[] = [
  { name: 'contextBridge', loader: () => require('../context-bridge') },
];

module.exports = {};

defineProperties(module.exports, btsModuleList);
