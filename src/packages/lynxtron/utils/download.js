import fs from 'node:fs';
import path from 'node:path';
import fetch from 'node-fetch';

/**
 * Download binary file (arrayBuffer version)
 * @param {string} url - URL of the file to download
 * @param {string} outputPath - Output file path
 * @param {Object} options - Download options
 * @param {number} options.timeoutMs - Download timeout in milliseconds
 * @returns {Promise<void>}
 */
export async function downloadBinary(url, outputPath, options = {}) {
  const { timeoutMs = 120000 } = options;

   // Ensure output directory exists
  const outputDir = path.dirname(outputPath);
  await fs.promises.mkdir(outputDir, { recursive: true });

  const controller = new AbortController();
  const signal = controller.signal;
  const timeoutId = setTimeout(() => controller.abort(), timeoutMs);

  try {
    const response = await fetch(url, { signal });
    clearTimeout(timeoutId);

    if (!response.ok) {
      throw new Error(`Download failed: HTTP ${response.status} ${response.statusText}`);
    }

     // Get content length if available
    const contentLength = response.headers.get('content-length');
    const totalBytes = contentLength ? parseInt(contentLength, 10) : null;

    // Read to memory
    const ab = await response.arrayBuffer();
    const buffer = Buffer.from(ab);

    // Write to file (synchronous one-time write)
    await fs.promises.writeFile(outputPath, buffer);

    // Console output progress information
    if (totalBytes) {
      process.stdout.write(`\nDownload completed: ${formatBytes(buffer.length)} / ${formatBytes(totalBytes)}\n`);
    } else {
      process.stdout.write(`\nDownload completed: ${formatBytes(buffer.length)}\n`);
    }
  } catch (error) {
    clearTimeout(timeoutId);

    // Clean up partially downloaded file
    try {
      await fs.promises.unlink(outputPath);
    } catch (_) {
      // Ignore cleanup failure
    }

    if (error.name === 'AbortError') {
      throw new Error(`Download timeout: Exceeded ${timeoutMs} milliseconds`);
    }
    throw error;
  }
}

export function formatBytes(bytes) {
  if (bytes === 0) return '0 Bytes';
  const k = 1024;
  const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return `${parseFloat((bytes / Math.pow(k, i)).toFixed(2))} ${sizes[i]}`;
}
