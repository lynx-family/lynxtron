const { contextBridge } = require('lynxtron');
const fs = require('node:fs/promises');

const www = async (message) => {
  return message + ' hhhh';
}

contextBridge.exposeInLynxBTS({
  nativeTSFN: () => require('./build/preload.node').run(),
  get: (message) => {
    return {
      from: 'contextBridge',
      message,
    };
  },

  hhhh: async(message) => {
    return message + ' hhhh';
  },

  heiheihei: async(message) => {
    console.log('heiheihei', await www(message));
    return message + ' heiheihei';
  },

  fileExists: async() => {
    try {
      await fs.access('./missing-file-for-promise-test');
      return true;
    } catch {
      return false;
    } finally {
      console.log('fileExists finally');
    }
  },

  fileApi: {
    fileExists: async() => {
      try {
        await fs.access('./missing-file-for-nested-promise-test');
        return true;
      } catch {
        return false;
      } finally {
        console.log('fileApi.fileExists finally');
      }
    },
  },
});
