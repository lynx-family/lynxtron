#!/usr/bin/env node

import proc from 'node:child_process';
import lynxtron from './lynxtron_bin.js';

const args = process.argv.slice(2);
console.log('spawning', lynxtron, process.argv[2]);
console.log("args", args);

const child = proc.spawn(lynxtron, args, { stdio: 'inherit', windowsHide: false });
child.on('close', function (code, signal) {
  if (code === null) {
    console.error(lynxtron, 'exited with signal', signal);
    process.exit(1);
  }
  process.exit(code);
});

const handleTerminationSignal = function (signal) {
  process.on(signal, function signalHandler () {
    if (!child.killed) {
      child.kill(signal);
    }
  });
};

handleTerminationSignal('SIGINT');
handleTerminationSignal('SIGTERM');
