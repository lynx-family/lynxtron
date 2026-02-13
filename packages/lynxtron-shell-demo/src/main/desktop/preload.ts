/**
 * PC Preload Script
 * 导出的内容会被自动映射到 NativeModules.nodejs
 */

import { contextBridge } from '@lynx-js/lynxtron/context-bridge';
contextBridge.exposeInLynxBTS({
  echo: (message: string) => {
    return `Echo from PC Service Thread: ${message}`;
  },
});

console.log('[PC Preload] Node.js capabilities exported');
