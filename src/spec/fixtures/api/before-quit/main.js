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
    if (app.commandLine.hasSwitch('will-quit-test')) {
      state.willQuitFired = false;
    }
    process.stdout.write(JSON.stringify(state));
    process.stdout.end();
    process.exit(0);
  }
});

app.on('will-quit', (event) => {
  state.order.push('will-quit');
  if (app.commandLine.hasSwitch('will-quit-test')) {
    state.willQuitFired = true;
    if (app.commandLine.hasSwitch('prevent-will-quit')) {
      event.preventDefault();
      state.prevented = true;
      process.stdout.write(JSON.stringify(state));
      process.stdout.end();
      process.exit(0);
    }
  }
  process.stdout.write(JSON.stringify(state));
  process.stdout.end();
});

app.on('ready', () => {
  app.quit();
});
