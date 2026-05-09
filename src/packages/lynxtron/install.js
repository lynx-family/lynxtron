import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { downloadBinary } from './utils/download.js';
import { BASE_URL, VERSION, ARCH, PLATFORM, PLATFROM_EXE_PATH } from './utils/env-config.js';

import extractZip from 'extract-zip';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);


const LYNXTRON_PATH = path.join(__dirname, "dist", PLATFROM_EXE_PATH);

const hasDownloadLynxtron = () => {
  return fs.existsSync(LYNXTRON_PATH);
}

// if lynxtron is already installed, exit.
if (hasDownloadLynxtron() && !process.env.npm_config_force_download) {
  console.log("lynxtron is already installed");
  process.exit(0);
}

const base_url = BASE_URL;

let downloadUrl = ''
if (process.env.npm_config_custom_lynxtron_binary_url) {
  console.log(`using custom lynxtron url: ${process.env.npm_config_custom_lynxtron_binary_url}`);
  downloadUrl = process.env.npm_config_custom_lynxtron_binary_url;
} else {
  if (!base_url) {
    console.log("lynxtron base url is empty");
    process.exit(0);
  }
  downloadUrl = `${base_url}/v${VERSION}/lynxtron-v${VERSION}-${PLATFORM}-${ARCH}.zip`
}


console.log(`downloading lynxtron from ${downloadUrl}`);

const PACKAGE_DIR_PATH = path.join(__dirname, "dist");
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

  // Restore macOS framework symlinks (extract-zip expands symlinks into copies)
  if (PLATFORM === 'darwin' || PLATFORM === 'mas') {
    try {
      const fwBase = path.join(PACKAGE_DIR_PATH, 'lynxtron.app', 'Contents', 'Frameworks', 'Lynxtron Framework.framework');
      const versionsDir = path.join(fwBase, 'Versions');
      // Find the actual version directory (e.g., "1.0")
      const versions = fs.readdirSync(versionsDir).filter(v => v !== 'Current');
      if (versions.length === 1) {
        const ver = versions[0];
        const verDir = path.join(versionsDir, ver);
        const currentLink = path.join(versionsDir, 'Current');
        const topBinary = path.join(fwBase, 'Lynxtron Framework');
        const topResources = path.join(fwBase, 'Resources');

        // Remove duplicates and create symlinks
        if (fs.existsSync(currentLink) && !fs.lstatSync(currentLink).isSymbolicLink()) {
          fs.rmSync(currentLink, { recursive: true });
          fs.symlinkSync(ver, currentLink);
          console.log(`Restored symlink: Versions/Current → ${ver}`);
        }
        if (fs.existsSync(topBinary) && !fs.lstatSync(topBinary).isSymbolicLink()) {
          fs.unlinkSync(topBinary);
          fs.symlinkSync(path.join('Versions', 'Current', 'Lynxtron Framework'), topBinary);
          console.log('Restored symlink: Lynxtron Framework → Versions/Current/Lynxtron Framework');
        }
        if (fs.existsSync(topResources) && !fs.lstatSync(topResources).isSymbolicLink()) {
          fs.rmSync(topResources, { recursive: true });
          fs.symlinkSync(path.join('Versions', 'Current', 'Resources'), topResources);
          console.log('Restored symlink: Resources → Versions/Current/Resources');
        }
      }
    } catch (symlinkError) {
      console.warn('Warning: Could not restore framework symlinks:', symlinkError.message);
      // Non-fatal — app still works with copies, just wastes disk space
    }
  }

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
