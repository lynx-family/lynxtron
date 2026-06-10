// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

export interface ResourceRequest {
  resourceType: string;
  scheme: string;
  url: string;
}

export interface ResourceResponse {
  url: string;
  statusCode: number;
  data: Buffer;
}

export type ProtocolHandler = (
  request: ResourceRequest
) => ResourceResponse | false | undefined;

export interface ProtocolRewriteRequest {
  scheme: string;
  url: string;
}

export type ProtocolRequestRewriter = (
  request: ProtocolRewriteRequest
) => string | false | undefined;

export interface Protocol {
  /**
   * Register a resource handler for a URL scheme such as `app`.
   *
   * Returning `false` or `undefined` falls back to the normal Lynx resource
   * loading path.
   */
  handle(scheme: string, handler: ProtocolHandler): void;
  /**
   * Whether a handler is registered for `scheme`.
   */
  isProtocolHandled(scheme: string): boolean;
  /**
   * Remove the handler registered for `scheme`.
   */
  unhandle(scheme: string): void;
  /**
   * Rewrite a resource request URL before protocol handlers and fallback
   * loaders run. Pass `null` or `undefined` to clear the current rewriter.
   */
  setRequestRewriter(handler: ProtocolRequestRewriter | null | undefined): void;
}

export declare const protocol: Protocol;
