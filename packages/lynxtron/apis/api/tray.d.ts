// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { EventEmitter } from 'node:events';
import { Menu } from './menu';
import { NativeImage } from './native-image';
import { Point } from '../structures/point';
import { Rectangle } from '../structures/rectangle';

export interface DisplayBalloonOptions {
  icon?: NativeImage | string;
  iconType?: 'none' | 'info' | 'warning' | 'error' | 'custom';
  title: string;
  content: string;
  largeIcon?: boolean;
  noSound?: boolean;
  respectQuietTime?: boolean;
}

export interface TitleOptions {
  fontType?: 'monospaced' | 'monospacedDigit';
}

export declare class Tray extends EventEmitter {
  constructor(image: NativeImage | string, guid?: string);
  destroy(): void;
  isDestroyed(): boolean;
  setImage(image: NativeImage | string): void;
  setPressedImage(image: NativeImage | string): void;
  setToolTip(toolTip: string): void;
  setTitle(title: string, options?: TitleOptions): void;
  getTitle(): string;
  setIgnoreDoubleClickEvents(ignore: boolean): void;
  getIgnoreDoubleClickEvents(): boolean;
  setContextMenu(menu: Menu | null): void;
  popUpContextMenu(menu?: Menu, position?: Point): void;
  closeContextMenu(): void;
  displayBalloon(options: DisplayBalloonOptions): void;
  removeBalloon(): void;
  focus(): void;
  getBounds(): Rectangle;
  getGUID(): string | null;
}
