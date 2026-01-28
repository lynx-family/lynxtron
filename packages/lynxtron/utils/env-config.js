import os from 'node:os';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

function getPlatformPath (platform) {
  switch (platform) {
    case 'mas':
    case 'darwin':
      return 'lynxtron.app/Contents/MacOS/lynxtron';
    // case 'freebsd':
    // case 'openbsd':
    // case 'linux':
    //   return 'lynxtron';
    case 'win32':
      return 'lynxtron/lynxtron.exe';
    default:
      throw new Error('lynxtron builds are not available on platform: ' + platform);
  }
}

// TODO(liting): find BASE_URL.
export const BASE_URL = '';

const pckJson = JSON.parse(fs.readFileSync(path.join(__dirname, "..", 'package.json'), 'utf8'));

export const VERSION = pckJson.version;

export const ARCH = process.env.npm_config_arch || process.arch;

export const PLATFORM = process.env.npm_config_platform || os.platform();

export const PLATFROM_EXE_PATH = getPlatformPath(PLATFORM);
