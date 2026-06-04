const { contextBridge } = require('lynxtron');

const www = async (message) => {
  return message + ' hhhh';
}

contextBridge.exposeInLynxBTS({
  get: (message) => {
    return message;
  },

  hhhh: async(message) => {
    return message + ' hhhh';
  },

  heiheihei: async(message) => {
    console.log('heiheihei', await www(message));
    return message + ' heiheihei';
  },
});
