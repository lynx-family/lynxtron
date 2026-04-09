const { app } = require('lynxtron');

process.on('message', () => {
  console.log('Notified to quit');
  app.quit();
});
