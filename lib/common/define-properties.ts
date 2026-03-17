// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

const handleESModule = (loader: LynxtronInternal.ModuleLoader) => () => {
  const value = loader();
  if (value.__esModule && value.default) return value.default;
  return value;
};

// Attaches properties to |targetExports|.
export function defineProperties(
  targetExports: Object,
  moduleList: LynxtronInternal.ModuleEntry[]
) {
  const descriptors: PropertyDescriptorMap = {};
  for (const module of moduleList) {
    descriptors[module.name] = {
      enumerable: true,
      get: handleESModule(module.loader),
    };
  }
  return Object.defineProperties(targetExports, descriptors);
}
