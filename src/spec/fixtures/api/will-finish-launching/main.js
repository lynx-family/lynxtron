const { app } = require('lynxtron');

const state = {
  fired: false,
  order: [],
};

app.on('will-finish-launching', () => {
  state.fired = true;
  state.order.push('will-finish-launching');
});

app.on('ready', () => {
  state.order.push('ready');
  process.stdout.write(JSON.stringify(state));
  process.stdout.end();
  app.quit();
});
