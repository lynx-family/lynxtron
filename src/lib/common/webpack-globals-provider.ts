// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Captures original globals into a scope to ensure that userland modifications do
// not impact Electron.  Note that users doing:
//
// global.Promise.resolve = myFn
//
// Will mutate this captured one as well and that is OK.

export const Promise = global.Promise;
