import { defineProperties } from '@lynxtron/internal/common/define-properties';

const btsModuleList: LynxtronInternal.ModuleEntry[] = [
  { name: 'contextBridge', loader: () => require('../context-bridge') },
];

module.exports = {};

defineProperties(module.exports, btsModuleList);
