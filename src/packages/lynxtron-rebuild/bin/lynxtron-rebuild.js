#!/usr/bin/env node

import { lynxtronRebuild } from '../lib/index.js';

function parseArgs() {
  const args = process.argv.slice(2);
  const options = {};

  for (let i = 0; i < args.length; i++) {
    if (args[i] === '--arch' && args[i + 1]) {
      options.arch = args[i + 1];
      i++;
    } else if (args[i] === '--help') {
      console.log('Usage: lynxtron-rebuild [options]');
      console.log('');
      console.log('Options:');
      console.log('  --arch <architecture>  Target architecture (e.g., x64, arm64, ia32)');
      console.log('                         Default: current system architecture');
      console.log('  --help                 Show this help message');
      process.exit(0);
    }
  }

  return options;
}

async function main() {
  try {
    const options = parseArgs();
    await lynxtronRebuild(options);
  } catch (err) {
    console.error('Error during rebuild:', err);
    process.exit(1);
  }
}

main();