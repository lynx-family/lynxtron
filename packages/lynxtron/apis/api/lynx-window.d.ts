// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { NativeImage } from './native-image';
import { BaseWindow } from './base-window';
import { Point } from '../structures/point';

export interface LynxWindowConstructorOptions {
  /**
   * Window's width in pixels. Default is `800`.
   */
  width?: number;
  /**
   * Window's height in pixels. Default is `600`.
   */
  height?: number;
  /**
   * (**required** if y is used) Window's left offset from screen. Default is to
   * center the window.
   */
  x?: number;
  /**
   * (**required** if x is used) Window's top offset from screen. Default is to
   * center the window.
   */
  y?: number;
  /**
   * The `width` and `height` would be used as web page's size, which means the
   * actual window's size will include window frame's size and be slightly larger.
   * Default is `false`.
   */
  useContentSize?: boolean;
  /**
   * Show window in the center of the screen.
   */
  center?: boolean;
  /**
   * Window's minimum width. Default is `0`.
   */
  minWidth?: number;
  /**
   * Window's minimum height. Default is `0`.
   */
  minHeight?: number;
  /**
   * Window's maximum width. Default is no limit.
   */
  maxWidth?: number;
  /**
   * Window's maximum height. Default is no limit.
   */
  maxHeight?: number;
  /**
   * Whether window is resizable. Default is `true`.
   */
  resizable?: boolean;
  /**
   * Whether window is movable. This is not implemented on Linux. Default is `true`.
   */
  movable?: boolean;
  /**
   * Whether window is minimizable. This is not implemented on Linux. Default is
   * `true`.
   */
  minimizable?: boolean;
  /**
   * Whether window is maximizable. This is not implemented on Linux. Default is
   * `true`.
   */
  maximizable?: boolean;
  /**
   * Whether window is closable. This is not implemented on Linux. Default is `true`.
   */
  closable?: boolean;
  /**
   * Whether the window can be focused. Default is `true`. On Windows setting
   * `focusable: false` also implies setting `skipTaskbar: true`. On Linux setting
   * `focusable: false` makes the window stop interacting with wm, so the window will
   * always stay on top in all workspaces.
   */
  focusable?: boolean;
  /**
   * Whether the window should always stay on top of other windows. Default is
   * `false`.
   */
  alwaysOnTop?: boolean;
  /**
   * Whether the window should show in fullscreen. When explicitly set to `false` the
   * fullscreen button will be hidden or disabled on macOS. Default is `false`.
   */
  fullscreen?: boolean;
  /**
   * Whether the window can be put into fullscreen mode. On macOS, also whether the
   * maximize/zoom button should toggle full screen mode or maximize window. Default
   * is `true`.
   */
  fullscreenable?: boolean;
  /**
   * Use pre-Lion fullscreen on macOS. Default is `false`.
   */
  simpleFullscreen?: boolean;
  /**
   * Whether to show the window in taskbar. Default is `false`.
   */
  skipTaskbar?: boolean;
  hiddenInMissionControl?: boolean;
  /**
   * Whether the window is in kiosk mode. Default is `false`.
   */
  kiosk?: boolean;
  /**
   * The window icon. On Windows it is recommended to use `ICO` icons to get best
   * visual effects, you can also leave it undefined so the executable's icon will be
   * used.
   */
  icon?: NativeImage | string;
  /**
   * Whether window should be shown when created. Default is `true`.
   */
  show?: boolean;
  /**
   * Whether the renderer should be active when `show` is `false` and it has just
   * been created.  In order for `document.visibilityState` to work correctly on
   * first load with `show: false` you should set this to `false`.  Setting this to
   * `false` will cause the `ready-to-show` event to not fire.  Default is `true`.
   */
  paintWhenInitiallyHidden?: boolean;
  /**
   * Specify `false` to create a [Frameless Window]. Default is `true`.
   */
  frame?: boolean;
  /**
   * Specify parent window. Default is `null`.
   */
  parent?: LynxWindow;
  /**
   * Whether this is a modal window. This only works when the window is a child
   * window. Default is `false`.
   */
  modal?: boolean;
  /**
   * Whether to hide cursor when typing. Default is `false`.
   */
  disableAutoHideCursor?: boolean;
  /**
   * Auto hide the menu bar unless the `Alt` key is pressed. Default is `false`.
   */
  autoHideMenuBar?: boolean;
  /**
   * Enable the window to be resized larger than screen. Only relevant for macOS, as
   * other OSes allow larger-than-screen windows by default. Default is `false`.
   */
  enableLargerThanScreen?: boolean;
  /**
   * Window's background color as a hexadecimal value, like `#66CD00` or `#FFF` or
   * `#80FFFFFF` (alpha in #AARRGGBB format is supported if `transparent` is set to
   * `true`). Default is `#FFF` (white).
   */
  backgroundColor?: string;
  /**
   * Whether window should have a shadow. Default is `true`.
   */
  hasShadow?: boolean;
  /**
   * Set the initial opacity of the window, between 0.0 (fully transparent) and 1.0
   * (fully opaque). This is only implemented on Windows and macOS.
   */
  opacity?: number;
  /**
   * Makes the window [transparent]. Default is `false`. On Windows, does not work
   * unless the window is frameless.
   */
  transparent?: boolean;
  /**
   * The type of window, default is normal window. See more about this below.
   */
  type?: string;
  /**
   * Specify how the material appearance should reflect window activity state on
   * macOS. Must be used with the `vibrancy` property. Possible values are:
   */
  visualEffectState?: 'followWindow' | 'active' | 'inactive';
  /**
   * The style of window title bar. Default is `default`. Possible values are:
   *
   * @platform darwin,win32
   */
  titleBarStyle?: 'default' | 'hidden' | 'hiddenInset' | 'customButtonsOnHover';
  /**
   * Set a custom position for the traffic light buttons in frameless windows.
   */
  trafficLightPosition?: Point;
  /**
   * Whether frameless window should have rounded corners on macOS. Default is
   * `true`.
   */
  roundedCorners?: boolean;
  /**
   * Shows the title in the title bar in full screen mode on macOS for `hiddenInset`
   * titleBarStyle. Default is `false`.
   *
   * @deprecated Use `titleBarStyle` instead.
   */
  fullscreenWindowTitle?: boolean;
  /**
   * Use `WS_THICKFRAME` style for frameless windows on Windows, which adds standard
   * window frame. Setting it to `false` will remove window shadow and window
   * animations. Default is `true`.
   */
  thickFrame?: boolean;
  /**
   * Add a type of vibrancy effect to the window, only on macOS. Can be
   * `appearance-based`, `light`, `dark`, `titlebar`, `selection`, `menu`, `popover`,
   * `sidebar`, `medium-light`, `ultra-dark`, `header`, `sheet`, `window`, `hud`,
   * `fullscreen-ui`, `tooltip`, `content`, `under-window`, or `under-page`. Please
   * note that `appearance-based`, `light`, `dark`, `medium-light`, and `ultra-dark`
   * are deprecated and have been removed in macOS Catalina (10.15).
   */
  vibrancy?:
    | 'appearance-based'
    | 'light'
    | 'dark'
    | 'titlebar'
    | 'selection'
    | 'menu'
    | 'popover'
    | 'sidebar'
    | 'medium-light'
    | 'ultra-dark'
    | 'header'
    | 'sheet'
    | 'window'
    | 'hud'
    | 'fullscreen-ui'
    | 'tooltip'
    | 'content'
    | 'under-window'
    | 'under-page';
  /**
   * Tab group name, allows opening the window as a native tab on macOS 10.12+.
   * Windows with the same tabbing identifier will be grouped together. This also
   * adds a native new tab button to your window's tab bar and allows your `app` and
   * window to receive the `new-window-for-tab` event.
   */
  tabbingIdentifier?: string;
  /**
   * Whether to follow parent window if `parent` is set.
   */
  followParent?: boolean;

  /**
   * Whether node integration is enabled. Default is undefined.
   * path is node init file for node env exports.
   */
  nodeIntegration?: {
    preload_paths: string[];
  };

  title?: string;
}

export declare class LynxWindow extends BaseWindow {
  /**
   * LynxWindow
   */
  constructor(options?: LynxWindowConstructorOptions);
  /**
   * The window with the given `id`.
   */
  static fromId(id: number): LynxWindow | null;
  /**
   * An array of all opened browser windows.
   */
  static getAllWindows(): LynxWindow[];
  /**
   * The window that is focused in this application, otherwise returns `null`.
   */
  static getFocusedWindow(): LynxWindow | null;
  tabbingIdentifier: string;
  moveAbove(mediaSourceId: string): void;
  getMediaSourceId(): string;
  /**
   * Load bundle data for the window.
   */
  loadFile(path: string): boolean;
  /**
   * Load bundle url for the window, eg: 'http://localhost:3000/main.lynx.bundle'
   */
  loadURL(url: string): boolean;
  /**
   * Update bundle data for the window.
   */
  updateData(data: Object, globalProps: Object): boolean;
  /**
   * Send global event to the window.
   */
  sendGlobalEvent(eventName: string, data: Object): boolean;
}
