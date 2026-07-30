// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

const http = require('http');
const https = require('https');

export type LynxHttpRequestOptions = {
  url: string;
  method?: string;
  headers?: Record<string, string>;
  body?: Buffer;
  timeoutMs?: number;
  maxBytes?: number;
  redirectLimit?: number;
};

export interface LynxHttpResponseData {
  url: string;
  statusCode: number;
  statusText: string;
  headers: Record<string, string>;
  data: Buffer;
}

const DEFAULT_TIMEOUT_MS = 15000;
const DEFAULT_MAX_BYTES = 10 * 1024 * 1024;
const DEFAULT_REDIRECT_LIMIT = 5;
const REDIRECT_STATUS_CODES = new Set([301, 302, 303, 307, 308]);
const BODY_HEADER_NAMES = new Set([
  'content-encoding',
  'content-language',
  'content-length',
  'content-location',
  'content-type',
  'transfer-encoding',
]);
const CREDENTIAL_HEADER_NAMES = new Set([
  'authorization',
  'cookie',
  'proxy-authorization',
]);

function normalizeResponseHeaders(
  headers: Record<string, string | string[] | undefined>
): Record<string, string> {
  const result: Record<string, string> = {};
  for (const [name, value] of Object.entries(headers)) {
    if (Array.isArray(value)) {
      result[name] = value.join(', ');
    } else if (typeof value === 'string') {
      result[name] = value;
    }
  }
  return result;
}

function prepareRedirect(
  statusCode: number,
  currentUrl: URL,
  redirectedUrl: URL,
  method: string,
  headers: Record<string, string>,
  body: Buffer | undefined
) {
  let redirectedMethod = method;
  let redirectedBody = body;
  const redirectedHeaders = { ...headers };

  if (
    (statusCode === 303 && method !== 'HEAD') ||
    ((statusCode === 301 || statusCode === 302) && method === 'POST')
  ) {
    redirectedMethod = 'GET';
    redirectedBody = undefined;
    for (const name of Object.keys(redirectedHeaders)) {
      if (BODY_HEADER_NAMES.has(name.toLowerCase())) {
        delete redirectedHeaders[name];
      }
    }
  }

  for (const name of Object.keys(redirectedHeaders)) {
    const lowerName = name.toLowerCase();
    if (lowerName === 'host') {
      delete redirectedHeaders[name];
    } else if (
      currentUrl.origin !== redirectedUrl.origin &&
      CREDENTIAL_HEADER_NAMES.has(lowerName)
    ) {
      delete redirectedHeaders[name];
    }
  }

  return {
    method: redirectedMethod,
    headers: redirectedHeaders,
    body: redirectedBody,
  };
}

export function requestHttpBuffer({
  url,
  method = 'GET',
  headers = {},
  body,
  timeoutMs = DEFAULT_TIMEOUT_MS,
  maxBytes = DEFAULT_MAX_BYTES,
  redirectLimit = DEFAULT_REDIRECT_LIMIT,
}: LynxHttpRequestOptions): Promise<LynxHttpResponseData> {
  const initialUrl = new URL(url);
  const initialMethod = method.toUpperCase() || 'GET';

  const requestOnce = (
    requestUrl: URL,
    requestMethod: string,
    requestHeaders: Record<string, string>,
    requestBody: Buffer | undefined,
    redirectsFollowed: number
  ): Promise<LynxHttpResponseData> =>
    new Promise((resolve, reject) => {
      if (requestUrl.protocol !== 'http:' && requestUrl.protocol !== 'https:') {
        reject(new Error(`Unsupported protocol: ${requestUrl.protocol}`));
        return;
      }

      const transport = requestUrl.protocol === 'https:' ? https : http;
      const req = transport.request(
        requestUrl,
        { method: requestMethod, headers: requestHeaders },
        (res: any) => {
          const statusCode =
            typeof res.statusCode === 'number' ? res.statusCode : 0;
          const location =
            typeof res.headers?.location === 'string'
              ? res.headers.location
              : '';

          if (REDIRECT_STATUS_CODES.has(statusCode) && location) {
            if (redirectsFollowed >= redirectLimit) {
              res.resume();
              reject(new Error(`Too many redirects: ${redirectLimit}`));
              return;
            }

            let redirectedUrl: URL;
            try {
              redirectedUrl = new URL(location, requestUrl);
            } catch (error) {
              res.resume();
              reject(error);
              return;
            }
            const redirectedRequest = prepareRedirect(
              statusCode,
              requestUrl,
              redirectedUrl,
              requestMethod,
              requestHeaders,
              requestBody
            );
            res.resume();
            requestOnce(
              redirectedUrl,
              redirectedRequest.method,
              redirectedRequest.headers,
              redirectedRequest.body,
              redirectsFollowed + 1
            ).then(resolve, reject);
            return;
          }

          const buffers: Buffer[] = [];
          let bytesRead = 0;
          res.on('data', (chunk: any) => {
            const buffer = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
            bytesRead += buffer.length;
            if (bytesRead > maxBytes) {
              req.destroy(
                new Error(`Response too large: more than ${maxBytes} bytes`)
              );
              return;
            }
            buffers.push(buffer);
          });
          res.on('end', () => {
            resolve({
              url: requestUrl.href,
              statusCode,
              statusText:
                typeof res.statusMessage === 'string' ? res.statusMessage : '',
              headers: normalizeResponseHeaders(res.headers || {}),
              data: Buffer.concat(buffers),
            });
          });
          res.on('error', reject);
        }
      );

      req.on('error', reject);
      req.setTimeout(timeoutMs, () => {
        req.destroy(new Error(`Request timeout after ${timeoutMs}ms`));
      });
      if (requestBody && requestBody.length > 0) {
        req.write(requestBody);
      }
      req.end();
    });

  return requestOnce(initialUrl, initialMethod, { ...headers }, body, 0);
}
