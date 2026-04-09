import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { downloadBinary } from '../utils/download.js';
import { BASE_URL, VERSION, ARCH, PLATFORM} from '../utils/env-config.js';
import extractZip from 'extract-zip';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const CEF_X_WEBVIEW_PATH = path.join(__dirname, "..", "dist", PLATFORM, ARCH);

const hasDownloadCefXWebview = () => {
  return fs.existsSync(CEF_X_WEBVIEW_PATH);
}

// if cef-x-webview is already installed, exit.
if (hasDownloadCefXWebview() && !process.env.npm_config_force_download) {
  console.log("cef-x-webview is already installed");
  process.exit(0);
}

if (process.env.npm_config_skip_cef_x_webview_download) {
  console.log("skip cef-x-webview download by env");
  process.exit(0);
}

const base_url = BASE_URL;

let downloadUrl = ''
if (process.env.npm_config_custom_cef_x_webview_binary_url) {
  console.log(`using custom cef-x-webview url: ${process.env.npm_config_custom_cef_x_webview_binary_url}`);
  downloadUrl = process.env.npm_config_custom_cef_x_webview_binary_url;
} else {
  if (!base_url) {
    console.log("cef-x-webview base url is empty");
    process.exit(0);
  }
  downloadUrl = `${base_url}/v${VERSION}/cef_x_webview-v${VERSION}-${PLATFORM}-${ARCH}.zip`
}

console.log(`downloading cef-x-webview from ${downloadUrl}`);

const PACKAGE_DIR_PATH = path.join(__dirname, "..", "dist", PLATFORM, ARCH);
const PACKAGE_PATH = path.join(PACKAGE_DIR_PATH, `${VERSION}.zip`);
if (fs.existsSync(PACKAGE_PATH)) {
  fs.rmSync(path.join(__dirname, "..", "dist"), { recursive: true, force: true });
}
fs.mkdirSync(PACKAGE_DIR_PATH, { recursive: true });

await downloadBinary(downloadUrl, PACKAGE_PATH, { timeoutMs: 120000 });

if (!fs.existsSync(PACKAGE_PATH)) {
  throw new Error("cef-x-webview download failed");
}

// Begin extract zip file
console.log(`Begin extract zip file: ${PACKAGE_PATH}`);

try {
  // Unzip file by extract-zip module
  const TMP_DIR = path.join(CEF_X_WEBVIEW_PATH, '_tmp_extract');
  if (!fs.existsSync(TMP_DIR)) {
    fs.mkdirSync(TMP_DIR, { recursive: true });
  }
  await extractZip(PACKAGE_PATH, { dir: TMP_DIR });
  const SRC_DIR = path.join(TMP_DIR, `cef_x_webview-v${VERSION}-${PLATFORM}-${ARCH}`);
  const items = fs.readdirSync(SRC_DIR);
  for (const item of items) {
    const srcPath = path.join(SRC_DIR, item);
    const destPath = path.join(CEF_X_WEBVIEW_PATH, item);
    fs.renameSync(srcPath, destPath);
  }
  fs.rmSync(TMP_DIR, { recursive: true, force: true });
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
  if (fs.existsSync(CEF_X_WEBVIEW_PATH)) {
    console.log('Install success: cef-x-webview has been successfully downloaded and unzipped');
  } else {
    throw new Error(`Unzip failed: Expected executable file ${CEF_X_WEBVIEW_PATH} not found`);
  }
} catch (extractError) {
  console.error('Error extracting zip file:', extractError);
  throw new Error('Install failed: Error extracting zip file');
}
