const { ipcRenderer } = require('lynxtron');

window.onload = function () {
  ipcRenderer.send('answer', process.argv);
};
