// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

export class LynxTemplateBundle {
  /**
   * A pre-decoded Lynx template bundle.
   *
   * This class corresponds to Lynx SDK's `LynxTemplateBundle` (TemplateBundle),
   * which is the output of Lynx's pre-decode capability. You can parse a Lynx
   * App Bundle ahead of time to obtain a TemplateBundle and then load it via
   * `LynxWindow.loadBundle(templateBundle)` for faster startup.
   *
   * Notes:
   * - A LynxTemplateBundle instance can be non-null but still invalid. Use
   *   `isValid()` to check and `getErrorMessage()` to inspect the parsing error.
   * - The constructor accepts either a Node.js `Buffer` or any `ArrayBufferView`
   *   containing the bundle bytes.
   */
  constructor(buffer: Buffer | ArrayBufferView);
  /**
   * Returns whether this TemplateBundle is valid.
   */
  isValid(): boolean;
  /**
   * Returns an error message when the TemplateBundle is invalid, otherwise an
   * empty string.
   */
  getErrorMessage(): string;
}

export default LynxTemplateBundle;
