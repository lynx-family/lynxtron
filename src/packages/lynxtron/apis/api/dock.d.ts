// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { Menu } from './menu';
import { NativeImage } from './native-image';

export declare class Dock {
  /**
   * an ID representing the request.
   *
   * When `critical` is passed, the dock icon will bounce until either the
   * application becomes active or the request is canceled.
   *
   * When `informational` is passed, the dock icon will bounce for one second.
   * However, the request remains active until either the application becomes active
   * or the request is canceled.
   *
   * > [!NOTE] This method can only be used while the app is not focused; when the
   * app is focused it will return -1.
   *
   * @platform darwin
   */
  bounce(type?: 'critical' | 'informational'): number;
  /**
   * Cancel the bounce of `id`.
   *
   * @platform darwin
   */
  cancelBounce(id: number): void;
  /**
   * Bounces the Downloads stack if the filePath is inside the Downloads folder.
   *
   * @platform darwin
   */
  downloadFinished(filePath: string): void;
  /**
   * The badge string of the dock.
   *
   * @platform darwin
   */
  getBadge(): string;
  /**
   * The application's dock menu.
   *
   * @platform darwin
   */
  getMenu(): Menu | null;
  /**
   * Hides the dock icon.
   *
   * @platform darwin
   */
  hide(): void;
  /**
   * Whether the dock icon is visible.
   *
   * @platform darwin
   */
  isVisible(): boolean;
  /**
   * Sets the string to be displayed in the dock’s badging area.
   *
   * > [!IMPORTANT] You need to ensure that your application has the permission to
   * display notifications for this method to work.
   *
   * @platform darwin
   */
  setBadge(text: string): void;
  /**
   * Sets the `image` associated with this dock icon.
   *
   * @platform darwin
   */
  setIcon(image: NativeImage | string): void;
  /**
   * Sets the application's dock menu.
   *
   * @platform darwin
   */
  setMenu(menu: Menu): void;
  /**
   * Resolves when the dock icon is shown.
   *
   * @platform darwin
   */
  show(): Promise<void>;
}
