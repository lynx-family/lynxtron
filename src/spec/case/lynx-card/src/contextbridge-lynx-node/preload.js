const { contextBridge } = require('lynxtron');
contextBridge.exposeInLynxBTS({
  get: (message) => {
    return {
        from: "contextBridge",
        message,
    }
  },
});
