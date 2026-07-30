// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

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
    const parsedUrl = new URL(urlString);
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
