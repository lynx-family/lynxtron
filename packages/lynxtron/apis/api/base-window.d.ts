// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { EventEmitter } from 'node:events';
import { Point } from '../structures/point';
import { Size } from '../structures/size';
import { Rectangle } from '../structures/rectangle';

export interface WillResizeDetails {
  /**
   * The edge of the window being dragged for resizing. Can be `bottom`, `left`,
   * `right`, `top-left`, `top-right`, `bottom-left` or `bottom-right`.
   */
  edge:
    | 'bottom'
    | 'left'
    | 'right'
    | 'top-left'
    | 'top-right'
    | 'bottom-left'
    | 'bottom-right';
}

export interface VisibleOnAllWorkspacesOptions {
  /**
   * Sets whether the window should be visible above fullscreen windows.
   *
   * @platform darwin
   */
  visibleOnFullScreen?: boolean;
  /**
   * Calling setVisibleOnAllWorkspaces will by default transform the process type
   * between UIElementApplication and ForegroundApplication to ensure the correct
   * behavior. However, this will hide the window and dock for a short time every
   * time it is called. If your window is already of type UIElementApplication, you
   * can bypass this transformation by passing true to skipTransformProcessType.
   *
   * @platform darwin
   */
  skipTransformProcessType?: boolean;
}

export interface BaseWindowConstructorOptions {
  /**
   * Whether the window should always stay on top of other windows. Default is
   * `false`.
   */
  alwaysOnTop?: boolean;
  /**
   * Auto hide the menu bar unless the `Alt` key is pressed. Default is `false`.
   *
   * @platform win32
   */
  autoHideMenuBar?: boolean;
  /**
   * The window's background color in Hex, RGB, RGBA, HSL, HSLA or named CSS color
   * format. Alpha in #AARRGGBB format is supported if `transparent` is set to
   * `true`. Default is `#FFF` (white). See win.setBackgroundColor for more
   * information.
   */
  backgroundColor?: string;
  /**
   * Set the window's system-drawn background material, including behind the
   * non-client area. Can be `auto`, `none`, `mica`, `acrylic` or `tabbed`. See
   * win.setBackgroundMaterial for more information.
   *
   * @platform win32
   */
  backgroundMaterial?: 'auto' | 'none' | 'mica' | 'acrylic' | 'tabbed';
  /**
   * Show window in the center of the screen. Default is `false`.
   */
  center?: boolean;
  /**
   * Whether window is closable. Default is `true`.
   *
   * @platform darwin,win32
   */
  closable?: boolean;
  /**
   * Whether to hide cursor when typing. Default is `false`.
   */
  disableAutoHideCursor?: boolean;
  /**
   * Enable the window to be resized larger than screen. Only relevant for macOS, as
   * other OSes allow larger-than-screen windows by default. Default is `false`.
   *
   * @platform darwin
   */
  enableLargerThanScreen?: boolean;
  /**
   * Whether the window can be focused. Default is `true`. On Windows setting
   * `focusable: false` also implies setting `skipTaskbar: true`.
   */
  focusable?: boolean;
  /**
   * Specify `false` to create a frameless window. Default is `true`.
   */
  frame?: boolean;
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
   * Whether window should have a shadow. Default is `true`.
   */
  hasShadow?: boolean;
  /**
   * Window's height in pixels. Default is `600`.
   */
  height?: number;
  /**
   * Whether window should be hidden when the user toggles into mission control.
   *
   * @platform darwin
   */
  hiddenInMissionControl?: boolean;
  /**
   * The window icon. On Windows it is recommended to use `ICO` icons to get best
   * visual effects, you can also leave it undefined so the executable's icon will be
   * used.
   */
  icon?: any; // NativeImage | string
  /**
   * Window's maximum height. Default is no limit.
   */
  maxHeight?: number;
  /**
   * Whether window is maximizable. Default is
   * `true`.
   *
   * @platform darwin,win32
   */
  maximizable?: boolean;
  /**
   * Window's maximum width. Default is no limit.
   */
  maxWidth?: number;
  /**
   * Window's minimum height. Default is `0`.
   */
  minHeight?: number;
  /**
   * Whether window is minimizable. Default is
   * `true`.
   *
   * @platform darwin,win32
   */
  minimizable?: boolean;
  /**
   * Window's minimum width. Default is `0`.
   */
  minWidth?: number;
  /**
   * Whether this is a modal window. This only works when the window is a child
   * window. Default is `false`.
   */
  modal?: boolean;
  /**
   * Whether window is movable. Default is `true`.
   *
   * @platform darwin,win32
   */
  movable?: boolean;
  /**
   * Set the initial opacity of the window, between 0.0 (fully transparent) and 1.0
   * (fully opaque). This is only implemented on Windows and macOS.
   *
   * @platform darwin,win32
   */
  opacity?: number;
  /**
   * Specify parent window. Default is `null`.
   */
  parent?: any; // BaseWindow
  /**
   * Whether window is resizable. Default is `true`.
   */
  resizable?: boolean;
  /**
   * Whether frameless window should have rounded corners. Default is `true`. Setting
   * this property to `false` will prevent the window from being fullscreenable on
   * macOS. On Windows versions older than Windows 11 Build 22000 this property has
   * no effect, and frameless windows will not have rounded corners.
   *
   * @platform darwin,win32
   */
  roundedCorners?: boolean;
  /**
   * Whether window should be shown when created. Default is `true`.
   */
  show?: boolean;
  /**
   * Use pre-Lion fullscreen on macOS. Default is `false`.
   *
   * @platform darwin
   */
  simpleFullscreen?: boolean;
  /**
   * Whether to show the window in taskbar. Default is `false`.
   *
   * @platform darwin,win32
   */
  skipTaskbar?: boolean;
  /**
   * Tab group name, allows opening the window as a native tab. Windows with the same
   * tabbing identifier will be grouped together. This also adds a native new tab
   * button to your window's tab bar and allows your `app` and window to receive the
   * `new-window-for-tab` event.
   *
   * @platform darwin
   */
  tabbingIdentifier?: string;
  /**
   * Use `WS_THICKFRAME` style for frameless windows on Windows, which adds standard
   * window frame. Setting it to `false` will remove window shadow and window
   * animations. Default is `true`.
   */
  thickFrame?: boolean;
  /**
   * Default window title. Default is `"Electron"`. If the HTML tag `<title>` is
   * defined in the HTML file loaded by `loadURL()`, this property will be ignored.
   */
  title?: string;
  /**
   * The style of window title bar. Default is `default`. Only supported on macOS.
   * Other platforms ignore this option.
   *
   * @platform darwin
   */
  titleBarStyle?: 'default' | 'hidden' | 'hiddenInset' | 'customButtonsOnHover';
  /**
   * Set a custom position for the traffic light buttons in frameless windows.
   *
   * @platform darwin
   */
  trafficLightPosition?: Point;
  /**
   * Makes the window transparent. Default is `false`. On Windows, does not work
   * unless the window is frameless.
   */
  transparent?: boolean;
  /**
   * The type of window, default is normal window. See more about this below.
   */
  type?: string;
  /**
   * The `width` and `height` would be used as web page's size, which means the
   * actual window's size will include window frame's size and be slightly larger.
   * Default is `false`.
   */
  useContentSize?: boolean;
  /**
   * Add a type of vibrancy effect to the window, only on macOS. Can be
   * `appearance-based`, `titlebar`, `selection`, `menu`, `popover`, `sidebar`,
   * `header`, `sheet`, `window`, `hud`, `fullscreen-ui`, `tooltip`, `content`,
   * `under-window`, or `under-page`.
   *
   * @platform darwin
   */
  vibrancy?:
    | 'appearance-based'
    | 'titlebar'
    | 'selection'
    | 'menu'
    | 'popover'
    | 'sidebar'
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
   * Specify how the material appearance should reflect window activity state on
   * macOS. Must be used with the `vibrancy` property. Possible values are:
   *
   * @platform darwin
   */
  visualEffectState?: 'followWindow' | 'active' | 'inactive';
  /**
   * Window's width in pixels. Default is `800`.
   */
  width?: number;
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
}

export declare class BaseWindow extends EventEmitter {
  /**
   * Emitted when an App Command is invoked. These are typically related to keyboard
   * media keys or browser commands, as well as the "Back" button built into some
   * mice on Windows.
   *
   * Commands are lowercased, underscores are replaced with hyphens, and the
   * `APPCOMMAND_` prefix is stripped off. e.g. `APPCOMMAND_BROWSER_BACKWARD` is
   * emitted as `browser-backward`.
   *
   * @platform win32
   */
  on(
    event: 'app-command',
    listener: (event: Event, command: string) => void
  ): this;
  /**
   * @platform win32
   */
  off(
    event: 'app-command',
    listener: (event: Event, command: string) => void
  ): this;
  /**
   * @platform win32
   */
  once(
    event: 'app-command',
    listener: (event: Event, command: string) => void
  ): this;
  /**
   * @platform win32
   */
  addListener(
    event: 'app-command',
    listener: (event: Event, command: string) => void
  ): this;
  /**
   * @platform win32
   */
  removeListener(
    event: 'app-command',
    listener: (event: Event, command: string) => void
  ): this;
  /**
   * Emitted when the window loses focus.
   */
  on(event: 'blur', listener: (event: Event) => void): this;
  off(event: 'blur', listener: (event: Event) => void): this;
  once(event: 'blur', listener: (event: Event) => void): this;
  addListener(event: 'blur', listener: (event: Event) => void): this;
  removeListener(event: 'blur', listener: (event: Event) => void): this;
  /**
   * Emitted when the window is going to be closed. It's emitted before the
   * `beforeunload` and `unload` event of the DOM. Calling `event.preventDefault()`
   * will cancel the close.
   */
  on(event: 'close', listener: (event: Event) => void): this;
  off(event: 'close', listener: (event: Event) => void): this;
  once(event: 'close', listener: (event: Event) => void): this;
  addListener(event: 'close', listener: (event: Event) => void): this;
  removeListener(event: 'close', listener: (event: Event) => void): this;
  /**
   * Emitted when the window is closed. After you have received this event you should
   * remove the reference to the window and avoid using it any more.
   */
  on(event: 'closed', listener: () => void): this;
  off(event: 'closed', listener: () => void): this;
  once(event: 'closed', listener: () => void): this;
  addListener(event: 'closed', listener: () => void): this;
  removeListener(event: 'closed', listener: () => void): this;
  /**
   * Emitted when the window enters a full-screen state.
   */
  on(event: 'enter-full-screen', listener: () => void): this;
  off(event: 'enter-full-screen', listener: () => void): this;
  once(event: 'enter-full-screen', listener: () => void): this;
  addListener(event: 'enter-full-screen', listener: () => void): this;
  removeListener(event: 'enter-full-screen', listener: () => void): this;
  /**
   * Emitted when the window gains focus.
   */
  on(event: 'focus', listener: (event: Event) => void): this;
  off(event: 'focus', listener: (event: Event) => void): this;
  once(event: 'focus', listener: (event: Event) => void): this;
  addListener(event: 'focus', listener: (event: Event) => void): this;
  removeListener(event: 'focus', listener: (event: Event) => void): this;
  /**
   * Emitted when the window is hidden.
   */
  on(event: 'hide', listener: () => void): this;
  off(event: 'hide', listener: () => void): this;
  once(event: 'hide', listener: () => void): this;
  addListener(event: 'hide', listener: () => void): this;
  removeListener(event: 'hide', listener: () => void): this;
  /**
   * Emitted when the window leaves a full-screen state.
   */
  on(event: 'leave-full-screen', listener: () => void): this;
  off(event: 'leave-full-screen', listener: () => void): this;
  once(event: 'leave-full-screen', listener: () => void): this;
  addListener(event: 'leave-full-screen', listener: () => void): this;
  removeListener(event: 'leave-full-screen', listener: () => void): this;
  /**
   * Emitted when window is maximized.
   */
  on(event: 'maximize', listener: () => void): this;
  off(event: 'maximize', listener: () => void): this;
  once(event: 'maximize', listener: () => void): this;
  addListener(event: 'maximize', listener: () => void): this;
  removeListener(event: 'maximize', listener: () => void): this;
  /**
   * Emitted when the window is minimized.
   */
  on(event: 'minimize', listener: () => void): this;
  off(event: 'minimize', listener: () => void): this;
  once(event: 'minimize', listener: () => void): this;
  addListener(event: 'minimize', listener: () => void): this;
  removeListener(event: 'minimize', listener: () => void): this;
  /**
   * Emitted when the window is being moved to a new position.
   */
  on(event: 'move', listener: () => void): this;
  off(event: 'move', listener: () => void): this;
  once(event: 'move', listener: () => void): this;
  addListener(event: 'move', listener: () => void): this;
  removeListener(event: 'move', listener: () => void): this;
  /**
   * Emitted once when the window is moved to a new position.
   *
   * > [!NOTE] On macOS, this event is an alias of `move`.
   *
   * @platform darwin,win32
   */
  on(event: 'moved', listener: () => void): this;
  /**
   * @platform darwin,win32
   */
  off(event: 'moved', listener: () => void): this;
  /**
   * @platform darwin,win32
   */
  once(event: 'moved', listener: () => void): this;
  /**
   * @platform darwin,win32
   */
  addListener(event: 'moved', listener: () => void): this;
  /**
   * @platform darwin,win32
   */
  removeListener(event: 'moved', listener: () => void): this;
  /**
   * Emitted after the window has been resized.
   */
  on(event: 'resize', listener: () => void): this;
  off(event: 'resize', listener: () => void): this;
  once(event: 'resize', listener: () => void): this;
  addListener(event: 'resize', listener: () => void): this;
  removeListener(event: 'resize', listener: () => void): this;
  /**
   * Emitted once when the window has finished being resized.
   *
   * This is usually emitted when the window has been resized manually. On macOS,
   * resizing the window with `setBounds`/`setSize` and setting the `animate`
   * parameter to `true` will also emit this event once resizing has finished.
   *
   * @platform darwin,win32
   */
  on(event: 'resized', listener: () => void): this;
  /**
   * @platform darwin,win32
   */
  off(event: 'resized', listener: () => void): this;
  /**
   * @platform darwin,win32
   */
  once(event: 'resized', listener: () => void): this;
  /**
   * @platform darwin,win32
   */
  addListener(event: 'resized', listener: () => void): this;
  /**
   * @platform darwin,win32
   */
  removeListener(event: 'resized', listener: () => void): this;
  /**
   * Emitted when the window is restored from a minimized state.
   */
  on(event: 'restore', listener: () => void): this;
  off(event: 'restore', listener: () => void): this;
  once(event: 'restore', listener: () => void): this;
  addListener(event: 'restore', listener: () => void): this;
  removeListener(event: 'restore', listener: () => void): this;
  /**
   * Emitted on trackpad rotation gesture. Continually emitted until rotation gesture
   * is ended. The `rotation` value on each emission is the angle in degrees rotated
   * since the last emission. The last emitted event upon a rotation gesture will
   * always be of value `0`. Counter-clockwise rotation values are positive, while
   * clockwise ones are negative.
   *
   * @platform darwin
   */
  on(
    event: 'rotate-gesture',
    listener: (event: Event, rotation: number) => void
  ): this;
  /**
   * @platform darwin
   */
  off(
    event: 'rotate-gesture',
    listener: (event: Event, rotation: number) => void
  ): this;
  /**
   * @platform darwin
   */
  once(
    event: 'rotate-gesture',
    listener: (event: Event, rotation: number) => void
  ): this;
  /**
   * @platform darwin
   */
  addListener(
    event: 'rotate-gesture',
    listener: (event: Event, rotation: number) => void
  ): this;
  /**
   * @platform darwin
   */
  removeListener(
    event: 'rotate-gesture',
    listener: (event: Event, rotation: number) => void
  ): this;
  /**
   * Emitted when a session is about to end due to a shutdown, machine restart, or
   * user log-off. Once this event fires, there is no way to prevent the session from
   * ending.
   *
   * @platform win32
   */
  on(event: 'session-end', listener: (event: any) => void): this; // WindowSessionEndEvent
  /**
   * @platform win32
   */
  off(event: 'session-end', listener: (event: any) => void): this;
  /**
   * @platform win32
   */
  once(event: 'session-end', listener: (event: any) => void): this;
  /**
   * @platform win32
   */
  addListener(event: 'session-end', listener: (event: any) => void): this;
  /**
   * @platform win32
   */
  removeListener(event: 'session-end', listener: (event: any) => void): this;
  /**
   * Emitted when the window opens a sheet.
   *
   * @platform darwin
   */
  on(event: 'sheet-begin', listener: () => void): this;
  /**
   * @platform darwin
   */
  off(event: 'sheet-begin', listener: () => void): this;
  /**
   * @platform darwin
   */
  once(event: 'sheet-begin', listener: () => void): this;
  /**
   * @platform darwin
   */
  addListener(event: 'sheet-begin', listener: () => void): this;
  /**
   * @platform darwin
   */
  removeListener(event: 'sheet-begin', listener: () => void): this;
  /**
   * Emitted when the window has closed a sheet.
   *
   * @platform darwin
   */
  on(event: 'sheet-end', listener: () => void): this;
  /**
   * @platform darwin
   */
  off(event: 'sheet-end', listener: () => void): this;
  /**
   * @platform darwin
   */
  once(event: 'sheet-end', listener: () => void): this;
  /**
   * @platform darwin
   */
  addListener(event: 'sheet-end', listener: () => void): this;
  /**
   * @platform darwin
   */
  removeListener(event: 'sheet-end', listener: () => void): this;
  /**
   * Emitted when the user clicks the native macOS new tab button. The new tab button
   * is only visible if the current window has a `tabbingIdentifier`.
   *
   * @platform darwin
   */
  on(event: 'new-window-for-tab', listener: (event: Event) => void): this;
  /**
   * @platform darwin
   */
  off(event: 'new-window-for-tab', listener: (event: Event) => void): this;
  /**
   * @platform darwin
   */
  once(event: 'new-window-for-tab', listener: (event: Event) => void): this;
  /**
   * @platform darwin
   */
  addListener(
    event: 'new-window-for-tab',
    listener: (event: Event) => void
  ): this;
  /**
   * @platform darwin
   */
  removeListener(
    event: 'new-window-for-tab',
    listener: (event: Event) => void
  ): this;
  /**
   * Emitted when the window is shown.
   */
  on(event: 'show', listener: () => void): this;
  off(event: 'show', listener: () => void): this;
  once(event: 'show', listener: () => void): this;
  addListener(event: 'show', listener: () => void): this;
  removeListener(event: 'show', listener: () => void): this;
  /**
   * Emitted on 3-finger swipe. Possible directions are `up`, `right`, `down`,
   * `left`.
   *
   * The method underlying this event is built to handle older macOS-style trackpad
   * swiping, where the content on the screen doesn't move with the swipe. Most macOS
   * trackpads are not configured to allow this kind of swiping anymore, so in order
   * for it to emit properly the 'Swipe between pages' preference in `System
   * Preferences > Trackpad > More Gestures` must be set to 'Swipe with two or three
   * fingers'.
   *
   * @platform darwin
   */
  on(event: 'swipe', listener: (event: Event, direction: string) => void): this;
  /**
   * @platform darwin
   */
  off(
    event: 'swipe',
    listener: (event: Event, direction: string) => void
  ): this;
  /**
   * @platform darwin
   */
  once(
    event: 'swipe',
    listener: (event: Event, direction: string) => void
  ): this;
  /**
   * @platform darwin
   */
  addListener(
    event: 'swipe',
    listener: (event: Event, direction: string) => void
  ): this;
  /**
   * @platform darwin
   */
  removeListener(
    event: 'swipe',
    listener: (event: Event, direction: string) => void
  ): this;
  /**
   * Emitted when the system context menu is triggered on the window, this is
   * normally only triggered when the user right clicks on the non-client area of
   * your window.  This is the window titlebar or any area you have declared as
   * `-webkit-app-region: drag` in a frameless window.
   *
   * Calling `event.preventDefault()` will prevent the menu from being displayed.
   *
   * To convert `point` to DIP, use `screen.screenToDipPoint(point)`.
   *
   * @platform win32
   */
  on(
    event: 'system-context-menu',
    listener: (
      event: Event,
      /**
       * The screen coordinates where the context menu was triggered.
       */
      point: Point
    ) => void
  ): this;
  /**
   * @platform win32
   */
  off(
    event: 'system-context-menu',
    listener: (
      event: Event,
      /**
       * The screen coordinates where the context menu was triggered.
       */
      point: Point
    ) => void
  ): this;
  /**
   * @platform win32
   */
  once(
    event: 'system-context-menu',
    listener: (
      event: Event,
      /**
       * The screen coordinates where the context menu was triggered.
       */
      point: Point
    ) => void
  ): this;
  /**
   * @platform win32
   */
  addListener(
    event: 'system-context-menu',
    listener: (
      event: Event,
      /**
       * The screen coordinates where the context menu was triggered.
       */
      point: Point
    ) => void
  ): this;
  /**
   * @platform win32
   */
  removeListener(
    event: 'system-context-menu',
    listener: (
      event: Event,
      /**
       * The screen coordinates where the context menu was triggered.
       */
      point: Point
    ) => void
  ): this;
  /**
   * Emitted when the window exits from a maximized state.
   */
  on(event: 'unmaximize', listener: () => void): this;
  off(event: 'unmaximize', listener: () => void): this;
  once(event: 'unmaximize', listener: () => void): this;
  addListener(event: 'unmaximize', listener: () => void): this;
  removeListener(event: 'unmaximize', listener: () => void): this;
  /**
   * Emitted before the window is moved. On Windows, calling `event.preventDefault()`
   * will prevent the window from being moved.
   *
   * Note that this is only emitted when the window is being moved manually. Moving
   * the window with `setPosition`/`setBounds`/`center` will not emit this event.
   *
   * @platform darwin,win32
   */
  on(
    event: 'will-move',
    listener: (
      event: Event,
      /**
       * Location the window is being moved to.
       */
      newBounds: Rectangle
    ) => void
  ): this;
  /**
   * @platform darwin,win32
   */
  off(
    event: 'will-move',
    listener: (
      event: Event,
      /**
       * Location the window is being moved to.
       */
      newBounds: Rectangle
    ) => void
  ): this;
  /**
   * @platform darwin,win32
   */
  once(
    event: 'will-move',
    listener: (
      event: Event,
      /**
       * Location the window is being moved to.
       */
      newBounds: Rectangle
    ) => void
  ): this;
  /**
   * @platform darwin,win32
   */
  addListener(
    event: 'will-move',
    listener: (
      event: Event,
      /**
       * Location the window is being moved to.
       */
      newBounds: Rectangle
    ) => void
  ): this;
  /**
   * @platform darwin,win32
   */
  removeListener(
    event: 'will-move',
    listener: (
      event: Event,
      /**
       * Location the window is being moved to.
       */
      newBounds: Rectangle
    ) => void
  ): this;
  /**
   * Emitted before the window is resized. Calling `event.preventDefault()` will
   * prevent the window from being resized.
   *
   * Note that this is only emitted when the window is being resized manually.
   * Resizing the window with `setBounds`/`setSize` will not emit this event.
   *
   * The possible values and behaviors of the `edge` option are platform dependent.
   * Possible values are:
   *
   * * On Windows, possible values are `bottom`, `top`, `left`, `right`, `top-left`,
   * `top-right`, `bottom-left`, `bottom-right`.
   * * On macOS, possible values are `bottom` and `right`.
   *   * The value `bottom` is used to denote vertical resizing.
   *   * The value `right` is used to denote horizontal resizing.
   *
   * @platform darwin,win32
   */
  on(
    event: 'will-resize',
    listener: (
      event: Event,
      /**
       * Size the window is being resized to.
       */
      newBounds: Rectangle,
      details: WillResizeDetails
    ) => void
  ): this;
  /**
   * @platform darwin,win32
   */
  off(
    event: 'will-resize',
    listener: (
      event: Event,
      /**
       * Size the window is being resized to.
       */
      newBounds: Rectangle,
      details: WillResizeDetails
    ) => void
  ): this;
  /**
   * @platform darwin,win32
   */
  once(
    event: 'will-resize',
    listener: (
      event: Event,
      /**
       * Size the window is being resized to.
       */
      newBounds: Rectangle,
      details: WillResizeDetails
    ) => void
  ): this;
  /**
   * @platform darwin,win32
   */
  addListener(
    event: 'will-resize',
    listener: (
      event: Event,
      /**
       * Size the window is being resized to.
       */
      newBounds: Rectangle,
      details: WillResizeDetails
    ) => void
  ): this;
  /**
   * @platform darwin,win32
   */
  removeListener(
    event: 'will-resize',
    listener: (
      event: Event,
      /**
       * Size the window is being resized to.
       */
      newBounds: Rectangle,
      details: WillResizeDetails
    ) => void
  ): this;

  /**
   * BaseWindow
   */
  constructor(options?: BaseWindowConstructorOptions);
  /**
   * The window with the given `id`.
   */
  static fromId(id: number): BaseWindow | null;
  /**
   * An array of all opened browser windows.
   */
  static getAllWindows(): BaseWindow[];
  /**
   * The window that is focused in this application, otherwise returns `null`.
   */
  static getFocusedWindow(): BaseWindow | null;
  /**
   * Removes focus from the window.
   */
  blur(): void;
  /**
   * Moves window to the center of the screen.
   */
  center(): void;
  /**
   * Try to close the window. This has the same effect as a user manually clicking
   * the close button of the window. The web page may cancel the close though. See
   * the close event.
   */
  close(): void;
  /**
   * Force closing the window, the `unload` and `beforeunload` event won't be emitted
   * for the web page, and `close` event will also not be emitted for this window,
   * but it guarantees the `closed` event will be emitted.
   */
  destroy(): void;
  /**
   * Starts or stops flashing the window to attract user's attention.
   */
  flashFrame(flag: boolean): void;
  /**
   * Focuses on the window.
   */
  focus(): void;
  /**
   * Whether the window is destroyed.
   */
  isDestroyed(): boolean;
  /**
   * Gets the background color of the window in Hex (`#RRGGBB`) format.
   *
   * See Setting `backgroundColor`.
   *
   * > [!NOTE] The alpha value is _not_ returned alongside the red, green, and blue
   * values.
   */
  getBackgroundColor(): string;
  /**
   * The `bounds` of the window as `Object`.
   *
   * > [!NOTE] On macOS, the y-coordinate value returned will be at minimum the Tray
   * height. For example, calling `win.setBounds({ x: 25, y: 20, width: 800, height:
   * 600 })` with a tray height of 38 means that `win.getBounds()` will return `{ x:
   * 25, y: 38, width: 800, height: 600 }`.
   */
  getBounds(): Rectangle;
  /**
   * All child windows.
   */
  getChildWindows(): BaseWindow[];
  /**
   * Contains the window's maximum width and height.
   */
  getMaximumSize(): number[];
  /**
   * Contains the window's minimum width and height.
   */
  getMinimumSize(): number[];
  /**
   * The platform-specific handle of the window.
   *
   * The native type of the handle is `HWND` on Windows, `NSView*` on macOS.
   */
  getNativeWindowHandle(): Buffer;
  /**
   * Contains the window bounds of the normal state
   *
   * > [!NOTE] Whatever the current state of the window : maximized, minimized or in
   * fullscreen, this function always returns the position and size of the window in
   * normal state. In normal state, getBounds and getNormalBounds returns the same
   * `Rectangle`.
   */
  getNormalBounds(): Rectangle;
  /**
   * between 0.0 (fully transparent) and 1.0 (fully opaque).
   */
  getOpacity(): number;
  /**
   * The parent window or `null` if there is no parent.
   */
  getParentWindow(): BaseWindow | null;
  /**
   * Contains the window's current position.
   */
  getPosition(): number[];
  /**
   * Contains the window's width and height.
   */
  getSize(): number[];
  /**
   * The title of the native window.
   *
   * > [!NOTE] The title of the web page can be different from the title of the
   * native window.
   */
  getTitle(): string;
  /**
   * Whether the window has a shadow.
   */
  hasShadow(): boolean;
  /**
   * Hides the window.
   */
  hide(): void;
  /**
   * Hooks a windows message. The `callback` is called when the message is received
   * in the WndProc.
   *
   * @platform win32
   */
  hookWindowMessage(
    message: number,
    callback: (wParam: Buffer, lParam: Buffer) => void
  ): void;
  /**
   * Whether the window is always on top of other windows.
   */
  isAlwaysOnTop(): boolean;
  /**
   * Whether the window can be manually closed by user.
   *
   * @platform darwin,win32
   */
  isClosable(): boolean;
  /**
   * Whether the window is destroyed.
   */
  isDestroyed(): boolean;
  /**
   * whether the window is enabled.
   */
  isEnabled(): boolean;
  /**
   * Whether the window can be focused.
   *
   * @platform darwin,win32
   */
  isFocusable(): boolean;
  /**
   * Adds `window` as a tab on this window, after the tab for the current window.
   *
   * @platform darwin
   */
  addTabbedWindow(window: BaseWindow): void;
  /**
   * Whether the window is focused.
   */
  isFocused(): boolean;
  /**
   * Whether the window is in fullscreen mode.
   */
  isFullScreen(): boolean;
  /**
   * Whether the maximize/zoom window button toggles fullscreen mode or maximizes the
   * window.
   */
  isFullScreenable(): boolean;
  isHiddenInMissionControl(): boolean;
  /**
   * Whether the window can be manually maximized by user.
   *
   * @platform darwin,win32
   */
  isMaximizable(): boolean;
  /**
   * Whether the window is maximized.
   */
  isMaximized(): boolean;
  /**
   * Whether menu bar automatically hides itself.
   *
   * @platform win32
   */
  isMenuBarAutoHide(): boolean;
  /**
   * Whether the menu bar is visible.
   *
   * @platform win32
   */
  isMenuBarVisible(): boolean;
  /**
   * Whether the window can be manually minimized by the user.
   *
   * @platform darwin,win32
   */
  isMinimizable(): boolean;
  /**
   * Whether the window is minimized.
   */
  isMinimized(): boolean;
  /**
   * Whether current window is a modal window.
   */
  isModal(): boolean;
  /**
   * Whether the window can be moved by user.
   *
   * @platform darwin,win32
   */
  isMovable(): boolean;
  /**
   * Whether the window is in normal state (not maximized, not minimized, not in
   * fullscreen mode).
   */
  isNormal(): boolean;
  /**
   * Whether the window can be manually resized by the user.
   */
  isResizable(): boolean;
  /**
   * Whether the window is in simple (pre-Lion) fullscreen mode.
   *
   * @platform darwin
   */
  isSimpleFullScreen(): boolean;
  /**
   * Whether the window is visible to the user in the foreground of the app.
   */
  isVisible(): boolean;
  /**
   * Whether the window is visible on all workspaces.
   *
   * > [!NOTE] This API always returns false on Windows.
   *
   * @platform darwin
   */
  isVisibleOnAllWorkspaces(): boolean;
  /**
   * `true` or `false` depending on whether the message is hooked.
   *
   * @platform win32
   */
  isWindowMessageHooked(message: number): boolean;
  /**
   * Maximizes the window. This will also show (but not focus) the window if it isn't
   * being displayed already.
   */
  maximize(): void;
  /**
   * Minimizes the window. On some platforms the minimized window will be shown in
   * the Dock.
   */
  minimize(): void;
  /**
   * Moves window to top(z-order) regardless of focus
   */
  moveTop(): void;
  /**
   * Restores the window from minimized state to its previous state.
   */
  restore(): void;
  /**
   * Sets whether the window should show always on top of other windows. After
   * setting this, the window is still a normal window, not a toolbox window which
   * can not be focused on.
   */
  setAlwaysOnTop(
    flag: boolean,
    level?:
      | 'normal'
      | 'floating'
      | 'torn-off-menu'
      | 'modal-panel'
      | 'main-menu'
      | 'status'
      | 'pop-up-menu'
      | 'screen-saver'
      | 'dock',
    relativeLevel?: number
  ): void;
  /**
   * This will make a window maintain an aspect ratio. The extra size allows a
   * developer to have space, specified in pixels, not included within the aspect
   * ratio calculations. This API already takes into account the difference between a
   * window's size and its content size.
   *
   * Consider a normal window with an HD video player and associated controls.
   * Perhaps there are 15 pixels of controls on the left edge, 25 pixels of controls
   * on the right edge and 50 pixels of controls below the player. In order to
   * maintain a 16:9 aspect ratio (standard aspect ratio for HD @1920x1080) within
   * the player itself we would call this function with arguments of 16/9 and {
   * width: 40, height: 50 }. The second argument doesn't care where the extra width
   * and height are within the content view--only that they exist. Sum any extra
   * width and height areas you have within the overall content view.
   *
   * The aspect ratio is not respected when window is resized programmatically with
   * APIs like `win.setSize`.
   *
   * To reset an aspect ratio, pass 0 as the `aspectRatio` value:
   * `win.setAspectRatio(0)`.
   */
  setAspectRatio(aspectRatio: number, extraSize?: Size): void;
  /**
   * Controls whether to hide cursor when typing.
   *
   * @platform darwin
   */
  setAutoHideCursor(autoHide: boolean): void;
  /**
   * Selects the previous tab when native tabs are enabled and there are other tabs
   * in the window.
   *
   * @platform darwin
   */
  selectPreviousTab(): void;
  /**
   * Selects the next tab when native tabs are enabled and there are other tabs in
   * the window.
   *
   * @platform darwin
   */
  selectNextTab(): void;
  /**
   * Sets whether the window menu bar should hide itself automatically. Once set the
   * menu bar will only show when users press the single `Alt` key.
   *
   * If the menu bar is already visible, calling `setAutoHideMenuBar(true)` won't
   * hide it immediately.
   *
   * @platform win32
   */
  setAutoHideMenuBar(hide: boolean): void;
  /**
   * Examples of valid `backgroundColor` values:
   *
   * * Hex
   *   * #fff (shorthand RGB)
   *   * #ffff (shorthand ARGB)
   *   * #ffffff (RGB)
   *   * #ffffffff (ARGB)
   * * RGB
   *   * `rgb\(([^\d]+),\s*([^\d]+),\s*([^\d]+)\)`
   *     * e.g. rgb(255, 255, 255)
   * * RGBA
   *   * `rgba\(([^\d]+),\s*([^\d]+),\s*([^\d]+),\s*([^\d.]+)\)`
   *     * e.g. rgba(255, 255, 255, 1.0)
   * * HSL
   *   * `hsl\((-?[^\d.]+),\s*([^\d.]+)%,\s*([^\d.]+)%\)`
   *     * e.g. hsl(200, 20%, 50%)
   * * HSLA
   *   * `hsla\((-?[^\d.]+),\s*([^\d.]+)%,\s*([^\d.]+)%,\s*([^\d.]+)\)`
   *     * e.g. hsla(200, 20%, 50%, 0.5)
   * * Color name
   *   * Options are listed in SkParseColor.cpp
   *   * Similar to CSS Color Module Level 3 keywords, but case-sensitive.
   *     * e.g. `blueviolet` or `red`
   *
   * Sets the background color of the window. See Setting `backgroundColor`.
   */
  setBackgroundColor(backgroundColor: string): void;
  /**
   * Resizes and moves the window to the supplied bounds. Any properties that are not
   * supplied will default to their current values.
   *
   * > [!NOTE] On macOS, the y-coordinate value cannot be smaller than the Tray
   * height. The tray height has changed over time and depends on the operating
   * system, but is between 20-40px. Passing a value lower than the tray height will
   * result in a window that is flush to the tray.
   */
  setBounds(bounds: Partial<Rectangle>, animate?: boolean): void;
  /**
   * Sets whether the window can be manually closed by user.
   *
   * @platform darwin,win32
   */
  setClosable(closable: boolean): void;
  /**
   * Disable or enable the window.
   */
  setEnabled(enable: boolean): void;
  /**
   * Changes whether the window can be focused.
   *
   * On macOS it does not remove the focus from the window.
   *
   * @platform darwin,win32
   */
  setFocusable(focusable: boolean): void;
  /**
   * Sets whether the window should be in fullscreen mode.
   *
   * > [!NOTE] On macOS, fullscreen transitions take place asynchronously. If further
   * actions depend on the fullscreen state, use the 'enter-full-screen' or >
   * 'leave-full-screen' events.
   */
  setFullScreen(flag: boolean): void;
  /**
   * Sets whether the maximize/zoom window button toggles fullscreen mode or
   * maximizes the window.
   */
  setFullScreenable(fullscreenable: boolean): void;
  setHiddenInMissionControl(hidden: boolean): void;
  /**
   * Shows or hides the tab overview when native tabs are enabled.
   *
   * @platform darwin
   */
  showAllTabs(): void;
  /**
   * Sets whether the window should have a shadow.
   */
  setHasShadow(hasShadow: boolean): void;
  /**
   * Merges all windows into one window with multiple tabs when native tabs are
   * enabled and there is more than one open window.
   *
   * @platform darwin
   */
  mergeAllWindows(): void;
  /**
   * Sets whether the window can be manually maximized by user.
   *
   * @platform darwin,win32
   */
  setMaximizable(maximizable: boolean): void;
  /**
   * Sets the maximum size of window to `width` and `height`.
   */
  setMaximumSize(width: number, height: number): void;
  /**
   * Sets whether the window can be manually minimized by user.
   *
   * @platform darwin,win32
   */
  setMinimizable(minimizable: boolean): void;
  /**
   * Sets the minimum size of window to `width` and `height`.
   */
  setMinimumSize(width: number, height: number): void;
  /**
   * Sets whether the window can be moved by user.
   *
   * @platform darwin,win32
   */
  setMovable(movable: boolean): void;
  /**
   * Moves the current tab into a new window if native tabs are enabled and there is
   * more than one tab in the current window.
   *
   * @platform darwin
   */
  moveTabToNewWindow(): void;
  /**
   * Sets the opacity of the window. Out of bound number
   * values are clamped to the [0, 1] range.
   *
   * @platform win32,darwin
   */
  setOpacity(opacity: number): void;
  /**
   * Sets `parent` as current window's parent window, passing `null` will turn
   * current window into a top-level window.
   */
  setParentWindow(parent: BaseWindow | null): void;
  /**
   * Moves window to `x` and `y`.
   */
  setPosition(x: number, y: number, animate?: boolean): void;
  /**
   * Sets progress value in progress bar. Valid range is [0, 1.0].
   *
   * Remove progress bar when progress < 0; Change to indeterminate mode when
   * progress > 1.
   *
   */
  setProgressBar(
    progress: number,
    options?: {
      /**
       * Mode for the progress bar. Can be `none`, `normal`, `indeterminate`, `error` or
       * `paused`. Default is `normal`.
       *
       * @platform win32,darwin
       */
      mode: 'none' | 'normal' | 'indeterminate' | 'error' | 'paused';
    }
  ): void;
  /**
   * Sets whether the window can be manually resized by the user.
   */
  setResizable(resizable: boolean): void;
  /**
   * Changes the attachment point for sheets on macOS. By default, sheets are
   * attached just below the window frame, but you may want to display them beneath a
   * HTML-rendered toolbar. For example:
   *
   * @platform darwin
   */
  setSheetOffset(offsetY: number, offsetX?: number): void;
  /**
   * Enters or leaves simple fullscreen mode.
   *
   * Simple fullscreen mode emulates the native fullscreen behavior found in versions
   * of macOS prior to Lion (10.7).
   *
   * @platform darwin
   */
  setSimpleFullScreen(flag: boolean): void;
  /**
   * Resizes the window to `width` and `height`. If `width` or `height` are below any
   * set minimum size constraints the window will snap to its minimum size.
   */
  setSize(width: number, height: number, animate?: boolean): void;
  /**
   * Contains the window's width and height.
   */
  getSize(): number[];
  /**
   * Sets the size of the content area.
   */
  setContentSize(width: number, height: number, animate?: boolean): void;
  /**
   * Gets the size of the content area.
   */
  getContentSize(): number[];
  /**
   * Resizes and moves the content area to the supplied bounds.
   */
  setContentBounds(bounds: Partial<Rectangle>, animate?: boolean): void;
  /**
   * Gets the bounds of the content area.
   */
  getContentBounds(): Rectangle;
  /**
   * Makes the window not show in the taskbar.
   *
   * @platform darwin,win32
   */
  setSkipTaskbar(skip: boolean): void;
  /**
   * Changes the title of native window to `title`.
   */
  setTitle(title: string): void;
  /**
   * Toggles the visibility of the tab bar if native tabs are enabled and there is
   * only one tab in the current window.
   *
   * @platform darwin
   */
  toggleTabBar(): void;
  /**
   * Adds a vibrancy effect to the window. Passing `null` or an empty string will
   * remove the vibrancy effect on the window.
   *
   * @platform darwin
   */
  setVibrancy(
    type:
      | (
          | 'titlebar'
          | 'selection'
          | 'menu'
          | 'popover'
          | 'sidebar'
          | 'header'
          | 'sheet'
          | 'window'
          | 'hud'
          | 'fullscreen-ui'
          | 'tooltip'
          | 'content'
          | 'under-window'
          | 'under-page'
        )
      | null
  ): void;
  /**
   * Sets whether the window should be visible on all workspaces.
   *
   * > [!NOTE] This API does nothing on Windows.
   *
   * @platform darwin
   */
  setVisibleOnAllWorkspaces(
    visible: boolean,
    options?: VisibleOnAllWorkspacesOptions
  ): void;
  /**
   * Set a custom position for the traffic light buttons in frameless window. Passing
   * `null` will reset the position to default.
   *
   * @platform darwin
   */
  setTrafficLightPosition(position: Point | null): void;
  /**
   * Sets whether the window traffic light buttons should be visible.
   *
   * @platform darwin
   */
  setWindowButtonVisibility(visible: boolean): void;
  /**
   * Shows and gives focus to the window.
   */
  show(): void;
  /**
   * Shows the window but doesn't focus on it.
   */
  showInactive(): void;
  /**
   * Unhooks all of the window messages.
   *
   * @platform win32
   */
  unhookAllWindowMessages(): void;
  /**
   * Unhook the window message.
   *
   * @platform win32
   */
  unhookWindowMessage(message: number): void;
  /**
   * Unmaximizes the window.
   */
  unmaximize(): void;
  /**
   * A `boolean` property that determines whether the window menu bar should hide
   * itself automatically. Once set, the menu bar will only show when users press the
   * single `Alt` key.
   *
   * If the menu bar is already visible, setting this property to `true` won't hide
   * it immediately.
   *
   * @platform win32
   */
  autoHideMenuBar: boolean;
  /**
   * A `boolean` property that determines whether the window can be manually closed
   * by user.
   *
   * @platform darwin,win32
   */
  closable: boolean;
  /**
   * A `boolean` property that determines whether the window is focusable.
   *
   * @platform win32,darwin
   */
  focusable: boolean;
  /**
   * A `boolean` property that determines whether the window is in fullscreen mode.
   */
  fullScreen: boolean;
  /**
   * A `boolean` property that determines whether the maximize/zoom window button
   * toggles fullscreen mode or maximizes the window.
   */
  fullScreenable: boolean;
  /**
   * A `boolean` property that determines whether the window can be manually
   * maximized by user.
   *
   * @platform darwin,win32
   */
  maximizable: boolean;
  /**
   * A `boolean` property that determines whether the window can be manually
   * minimized by user.
   *
   * @platform darwin,win32
   */
  minimizable: boolean;
  /**
   * A `boolean` property that determines Whether the window can be moved by user.
   *
   * @platform darwin,win32
   */
  movable: boolean;
  /**
   * A `boolean` property that determines whether the window can be manually resized
   * by user.
   */
  resizable: boolean;
  /**
   * A `boolean` property that determines whether the window has a shadow.
   */
  shadow: boolean;
  /**
   * A `boolean` property that determines whether the window is in simple (pre-Lion)
   * fullscreen mode.
   */
  simpleFullScreen: boolean;
  /**
   * A `string` property that determines the title of the native window.
   *
   * > [!NOTE] The title of the web page can be different from the title of the
   * native window.
   */
  title: string;
  /**
   * A `boolean` property that determines whether the window is visible on all
   * workspaces.
   *
   * > [!NOTE] Always returns false on Windows.
   *
   * @platform darwin
   */
  visibleOnAllWorkspaces: boolean;
  /**
   * A `boolean` property that determines whether the window is excluded from the
   * application’s Windows menu. `false` by default.
   *
   * @platform darwin
   */
  excludedFromShownWindowsMenu: boolean;
  /**
   * A `Integer` property representing the unique ID of the window. Each ID is unique
   * among all `BaseWindow` instances of the entire application.
   *
   */
  readonly id: number;
}
