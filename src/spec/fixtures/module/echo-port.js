const { parentPort } = require('worker_threads');

process.on('message', (msg) => {
  console.log('Received message:', msg);
  console.log('msg.port type:', typeof msg.port);
  console.log('msg.port constructor:', msg.port ? msg.port.constructor.name : 'N/A');
  if (msg && msg.port) {
    try {
      msg.port.postMessage('pong');
      msg.port.close();
    } catch (e) {
      console.error('Error posting message:', e);
    }
  }
});
