import { defineProperties } from '@lynxtron/internal/common/define-properties';

const btsModuleList: ElectronInternal.ModuleEntry[] = [
  { name: 'contextBridge', loader: () => require('../context-bridge') },
  { name: 'preloadRunner', loader: () => require('../preload-runner') },
];

module.exports = {};

defineProperties(module.exports, btsModuleList);
