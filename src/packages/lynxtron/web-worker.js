/**
 * Web Worker implementation of Lynxtron API
 */
export const contextBridge = {
  exposeInLynxBTS(exports) {
    if (globalThis.contextBridge) {
      globalThis.contextBridge.exposeInLynxBTS(exports);
    } else {
      globalThis.__lynxtron_nodejs_exports__ = exports;
    }
  }
};
