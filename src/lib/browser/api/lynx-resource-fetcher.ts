// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import * as fs from 'fs';
import * as path from 'path';
import { fileURLToPath } from 'url';

import { requestHttpBuffer } from './lynx-http-client';

export interface LynxFetchReplayData {
  url: string;
  statusCode: number;
  data: Buffer;
}

export interface LynxFetchEvent {
  sendReply: (arg: LynxFetchReplayData) => void;
}

export async function onResourceFetcher(
  event: LynxFetchEvent,
  _resourceType: string,
  url: string
): Promise<void> {
  const urlString = typeof url === 'string' ? url : String(url ?? '');

  try {
    // Lynx loads its background JS bootstrap script via assets://lynx_core.js.
    // Resolve this URL to the file packaged under process.resourcesPath.
    // Without it, loadCard is unavailable and background app initialization fails:
    // the first screen may still render through Lepus, but useEffect and JS event
    // handlers do not run.
    const assetsScheme = 'assets://';
    if (urlString.startsWith(assetsScheme)) {
      if (!process.resourcesPath) {
        throw new Error('resourcesPath is unavailable');
      }
      const base = path.resolve(process.resourcesPath);
      const resolved = path.resolve(base, urlString.slice(assetsScheme.length));
      const relative = path.relative(base, resolved);
      if (
        relative === '..' ||
        relative.startsWith(`..${path.sep}`) ||
        path.isAbsolute(relative)
      ) {
        throw new Error('asset path escapes resources dir');
      }
      const data = await fs.promises.readFile(resolved);
      event.sendReply({ url: urlString, statusCode: 0, data });
      return;
    }

    const parsedUrl = new URL(urlString);
    if (parsedUrl.protocol === 'file:') {
      const data = await fs.promises.readFile(fileURLToPath(parsedUrl));
      event.sendReply({ url: parsedUrl.href, statusCode: 0, data });
      return;
    }

    if (parsedUrl.protocol !== 'http:' && parsedUrl.protocol !== 'https:') {
      const empty = Buffer.alloc(0);
      event.sendReply({ url: urlString, statusCode: 1, data: empty });
      console.log(
        'on-fetch-resource: Unsupported protocol: ',
        parsedUrl.protocol
      );
      return;
    }

    const result = await requestHttpBuffer({ url: parsedUrl.href });
    const code = result.statusCode === 200 ? 0 : result.statusCode || 1;
    event.sendReply({ url: result.url, statusCode: code, data: result.data });
    console.log('on-fetch-resource: Success: ');
    return;
  } catch (e) {
    const empty = Buffer.alloc(0);
    event.sendReply({ url: urlString, statusCode: 1, data: empty });
    console.log('on-fetch-resource: Error: ', e);
    return;
  }
}
