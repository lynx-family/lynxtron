// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

const http = require('http');
const https = require('https');

type HttpFetchOptions = {
  timeoutMs?: number;
  maxBytes?: number;
  redirectLimit?: number;
  headers?: Record<string, string>;
};

export interface LynxFetchReplayData {
  url: string;
  statusCode: number;
  data: Buffer;
}

export interface LynxFetchEvent {
  sendReply: (arg: LynxFetchReplayData) => void;
}

const DEFAULT_TIMEOUT_MS = 15000;
const DEFAULT_MAX_BYTES = 10 * 1024 * 1024;
const DEFAULT_REDIRECT_LIMIT = 5;

function fetchHttpBuffer(
  url: string,
  options: HttpFetchOptions = {}
): Promise<LynxFetchReplayData> {
  const {
    timeoutMs = DEFAULT_TIMEOUT_MS,
    maxBytes = DEFAULT_MAX_BYTES,
    redirectLimit = DEFAULT_REDIRECT_LIMIT,
    headers,
  } = options;
  const initialUrl = new URL(url);
  const requestOnce = (
    requestUrl: string,
    redirectsFollowed: number
  ): Promise<LynxFetchReplayData> =>
    new Promise((resolve, reject) => {
      const parsedUrl = new URL(requestUrl);
      const transport = parsedUrl.protocol === 'https:' ? https : http;

      const requestOptions: any = { method: 'GET' };
      if (headers) {
        requestOptions.headers = headers;
      }

      const req = transport.request(parsedUrl, requestOptions, (res: any) => {
        const statusCode =
          typeof res.statusCode === 'number' ? res.statusCode : 0;
        const headers = (res.headers || {}) as Record<string, any>;
        const location =
          typeof headers.location === 'string' ? headers.location : '';

        if ([301, 302, 303, 307, 308].includes(statusCode) && location) {
          if (redirectsFollowed >= redirectLimit) {
            res.resume();
            reject(new Error(`Too many redirects: ${redirectLimit}`));
            return;
          }
          const redirectedUrl = new URL(location, parsedUrl).href;
          res.resume();
          requestOnce(redirectedUrl, redirectsFollowed + 1).then(
            resolve,
            reject
          );
          return;
        }

        const buffers: Buffer[] = [];
        let bytesRead = 0;
        res.on('data', (chunk: any) => {
          const buf = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
          bytesRead += buf.length;
          if (bytesRead > maxBytes) {
            req.destroy(new Error(`Response too large: > ${maxBytes} bytes`));
            return;
          }
          buffers.push(buf);
        });
        res.on('end', () => {
          const buf = Buffer.concat(buffers);
          resolve({
            url: parsedUrl.href,
            statusCode,
            data: buf,
          });
        });
        res.on('error', reject);
      });

      req.on('error', reject);
      req.setTimeout(timeoutMs, () => {
        req.destroy(new Error(`Request timeout after ${timeoutMs}ms`));
      });
      req.end();
    });

  return requestOnce(initialUrl.href, 0);
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

    const result = await fetchHttpBuffer(parsedUrl.href);
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
