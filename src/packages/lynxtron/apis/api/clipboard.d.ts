// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { NativeImage } from './native-image';

export type ClipboardType = 'clipboard';

export interface ClipboardWriteData {
  text?: string;
  html?: string;
  image?: NativeImage;
}

export interface Clipboard {
  /**
   * An array of supported formats for the clipboard contents.
   */
  availableFormats(type?: ClipboardType): string[];
  /**
   * Clears the clipboard contents.
   */
  clear(type?: ClipboardType): void;
  /**
   * The content in the clipboard as markup.
   */
  readHTML(type?: ClipboardType): string;
  /**
   * The image content in the clipboard.
   */
  readImage(type?: ClipboardType): NativeImage;
  /**
   * The content in the clipboard as plain text.
   */
  readText(type?: ClipboardType): string;
  /**
   * Writes data to the clipboard.
   */
  write(data: ClipboardWriteData, type?: ClipboardType): void;
  /**
   * Writes markup to the clipboard.
   */
  writeHTML(markup: string, type?: ClipboardType): void;
  /**
   * Writes an image to the clipboard.
   */
  writeImage(image: NativeImage, type?: ClipboardType): void;
  /**
   * Writes text to the clipboard as plain text.
   */
  writeText(text: string, type?: ClipboardType): void;
}

export declare const clipboard: Clipboard;
