/**
 * Node.js Simulation Environment for Web (Background Thread)
 */

import { contextBridge } from '@lynx-js/lynxtron/context-bridge';

contextBridge.exposeInLynxBTS({
  // Adapt to desktop's getExposed pattern
  echo(message: string) {
    return `Echo from Web BG Thread: ${message}`;
  },
});

console.log(
  '[Lynxtron Web] Node.js environment simulation initialized in BG Thread'
);
