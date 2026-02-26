interface ContextBridge {
  exposeInLynxBTS(newApis: APIS): void;
}

declare global {
  var __contextBridge: ContextBridge;
}

const contextBridge = {
  exposeInLynxBTS: (apis: object): void => {
    const fn = globalThis.__contextBridge?.exposeInLynxBTS;
    if (typeof fn !== 'function')
      throw new Error(
        'globalThis.__contextBridge.exposeInLynxBTS is not a function'
      );
    fn(apis);
  },
};

export default contextBridge;
