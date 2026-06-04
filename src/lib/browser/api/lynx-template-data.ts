// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

export class LynxTemplateData<T extends object = Record<string, unknown>> {
  private readonly value: T;

  constructor(value: T) {
    if (value == null || typeof value !== 'object' || Array.isArray(value)) {
      throw new TypeError('LynxTemplateData requires a plain object');
    }
    this.value = Object.freeze({ ...(value as any) });
  }

  toObject(): T {
    return this.value;
  }
}

export default LynxTemplateData;
