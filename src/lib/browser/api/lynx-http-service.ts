// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { LynxHttpResponseData, requestHttpBuffer } from './lynx-http-client';

export interface LynxHttpRequestData {
  url: string;
  method: string;
  headers: Record<string, string>;
  body: Buffer | Uint8Array;
}

type NativeBinding = {
  setRequestHandler: (
    handler: (
      request: LynxHttpRequestData,
      reply: (response: LynxHttpResponseData) => void
    ) => void
  ) => void;
};

const binding = process._linkedBinding(
  'lynxtron_binding_lynx_http_service'
) as NativeBinding;

export async function handleLynxHttpRequest(
  request: LynxHttpRequestData
): Promise<LynxHttpResponseData> {
  try {
    const result = await requestHttpBuffer({
      url: request.url,
      method: request.method,
      headers: request.headers,
      body: Buffer.isBuffer(request.body)
        ? request.body
        : Buffer.from(request.body),
    });
    return result;
  } catch (error) {
    return {
      url: request.url,
      statusCode: -1,
      statusText: error instanceof Error ? error.message : String(error),
      headers: {},
      data: Buffer.alloc(0),
    };
  }
}

binding.setRequestHandler((request, reply) => {
  void handleLynxHttpRequest(request).then(reply);
});
