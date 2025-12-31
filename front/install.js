import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { downloadBinary } from './utils/download.js';
import { BASE_URL, VERSION, ARCH, PLATFORM, PLATFROM_EXE_PATH } from './utils/env-config.js';

import extractZip from 'extract-zip';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);


const LYNXTRON_PATH = path.join(__dirname, "dist", PLATFORM, ARCH, PLATFROM_EXE_PATH);

const hasDownloadLynxtron = () => {
  return fs.existsSync(LYNXTRON_PATH);
}

// if lynxtron is already installed, exit.
if (hasDownloadLynxtron() && !process.env.npm_config_force_download) {
  console.log("lynxtron is already installed");
  process.exit(0);
}

const base_url = process.env.npm_config_lynxtron_binary_mirror || BASE_URL;

let downloadUrl = ''
if (process.env.npm_config_custom_lynxtron_binary_url) {
  console.log(`using custom lynxtron url: ${process.env.npm_config_custom_lynxtron_binary_url}`);
  downloadUrl = process.env.npm_config_custom_lynxtron_binary_url;
} else {
  downloadUrl = `${base_url}/v${VERSION}/lynxtron-v${VERSION}-${PLATFORM}-${ARCH}.zip`
}

console.log(`downloading lynxtron from ${downloadUrl}`);

const PACKAGE_DIR_PATH = path.join(__dirname, "dist", PLATFORM, ARCH);
const PACKAGE_PATH = path.join(PACKAGE_DIR_PATH, `${VERSION}.zip`);
if (fs.existsSync(PACKAGE_PATH)) {
  fs.rmSync(path.join(__dirname, "dist"), { recursive: true, force: true });
}
fs.mkdirSync(PACKAGE_DIR_PATH, { recursive: true });

await downloadBinary(downloadUrl, PACKAGE_PATH, { timeoutMs: 120000 });

if (!fs.existsSync(PACKAGE_PATH)) {
  throw new Error("lynxtron download failed");
}

// Begin extract zip file
console.log(`Begin extract zip file: ${PACKAGE_PATH}`);

try {
  // Unzip file by extract-zip module
  await extractZip(PACKAGE_PATH, { dir: PACKAGE_DIR_PATH });
  console.log('Unzip completed');

  // Delete original zip file after unzip to release space
  try {
    fs.unlinkSync(PACKAGE_PATH);
    console.log(`Deleted temporary zip file: ${PACKAGE_PATH}`);
  } catch (deleteError) {
    console.warn('Error deleting zip file:', deleteError);
    // Here we don't throw an error because unzip is already successful, and delete failure does not affect the main function
  }

  // Verify unzip success
  if (fs.existsSync(LYNXTRON_PATH)) {
    console.log('Install success: lynxtron has been successfully downloaded and unzipped');
    // For macOS app, may need to set executable permission
    if (PLATFORM === 'darwin' || PLATFORM === 'mas') {
      try {
        fs.chmodSync(LYNXTRON_PATH, 0o755);
        console.log('Set executable permission successfully');
      } catch (chmodError) {
        console.warn('Error setting executable permission:', chmodError);
      }
    }
  } else {
    throw new Error(`Unzip failed: Expected executable file ${LYNXTRON_PATH} not found`);
  }
} catch (extractError) {
  console.error('Error extracting zip file:', extractError);
  throw new Error('Install failed: Error extracting zip file');
}
