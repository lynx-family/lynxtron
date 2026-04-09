process.on('message', (msg) => {
  process.send(msg);
});
