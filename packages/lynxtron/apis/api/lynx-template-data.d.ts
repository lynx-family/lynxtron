// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

export class LynxTemplateData<T extends object = Record<string, unknown>> {
  constructor(value: T);
  toObject(): T;
}

export default LynxTemplateData;
