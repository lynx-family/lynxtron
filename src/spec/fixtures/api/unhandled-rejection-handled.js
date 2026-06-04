const { app } = require('lynxtron');

const handleUnhandledRejection = (reason) => {
  console.error(`Unhandled Rejection: ${reason.stack}`);
  app.quit();
};

const main = async () => {
  process.on('unhandledRejection', handleUnhandledRejection);
  throw new Error('oops');
};

main();
