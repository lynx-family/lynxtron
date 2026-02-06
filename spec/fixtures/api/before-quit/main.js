const { app } = require('lynxtron');

const state = {
  fired: false,
  order: [],
  prevented: false,
};

app.on('before-quit', (event) => {
  state.fired = true;
  state.order.push('before-quit');
  if (app.commandLine.hasSwitch('prevent-default')) {
    event.preventDefault();
    state.prevented = true;
    process.stdout.write(JSON.stringify(state));
    process.stdout.end();
    process.exit(0);
  }
});

app.on('will-quit', () => {
  state.order.push('will-quit');
  process.stdout.write(JSON.stringify(state));
  process.stdout.end();
});

app.on('ready', () => {
  app.quit();
});
