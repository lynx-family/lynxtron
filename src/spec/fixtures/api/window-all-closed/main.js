const { app, LynxWindow } = require('lynxtron');

let handled = false;

if (app.commandLine.hasSwitch('handle-event')) {
  app.on('window-all-closed', () => {
    handled = true;
    app.quit();
  });
}

app.on('quit', () => {
  process.stdout.write(JSON.stringify(handled));
  process.stdout.end();
});

app.whenReady().then(() => {
  const win = new LynxWindow();
  win.close();
});
