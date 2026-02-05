import os from 'node:os';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

export const BASE_URL = '';

const pckJson = JSON.parse(fs.readFileSync(path.join(__dirname, "..", "..", 'package.json'), 'utf8'));

export const VERSION = pckJson.version;

export const ARCH = process.env.npm_config_arch || process.arch;

export const PLATFORM = process.env.npm_config_platform || os.platform();
