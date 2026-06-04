const { app } = require('lynxtron');

app.on('quit', (event, exitCode) => {
  process.stdout.write(JSON.stringify({ exitCode }));
  process.stdout.end();
});

app.whenReady().then(() => {
  setImmediate(() => {
    app.exit(3);
  });
});
