const { app } = require('lynxtron');

function output(value) {
  process.stdout.write(JSON.stringify(value));
  process.stdout.end();
}

app.whenReady().then(() => {
  if (app.commandLine.hasSwitch('set-options')) {
    app.setAboutPanelOptions({
      applicationName: 'electron!!',
      version: '1.2.3',
    });
  }

  if (app.commandLine.hasSwitch('show-panel')) {
    app.showAboutPanel();
  }

  setImmediate(() => {
    output(true);
    app.quit();
  });
});
