// --- Types ---
export interface APIS {
  [key: string]: unknown;
}

interface ContextBridge {
  exposeInLynxBTS(newApis: APIS): void;
}

declare global {
  var __contextBridge: ContextBridge;
}

const contextBridge = {
  exposeInLynxBTS: (newApis: APIS): void => {
    const fn = globalThis.__contextBridge?.exposeInLynxBTS;
    if (typeof fn !== 'function')
      throw new Error(
        'globalThis.__contextBridge.exposeInLynxBTS is not a function'
      );
    fn(newApis);
  },
};

export default contextBridge;
