// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { NativeImage } from './native-image';
import { NotificationResponse } from './notification-response';
import { ProcessMetric } from './process-metric';
import { JumpListSettings, JumpListCategory } from './jump-list-item';
import { Task } from './task';
import { CommandLine } from './command-line';
import { Menu } from './menu';
import { Dock } from './dock';
import { EventEmitter } from 'node:events';
import { LynxWindow } from './lynx-window';

export interface MoveToApplicationsFolderOptions {
  /**
   * A handler for potential conflict in move failure.
   */
  conflictHandler?: (conflictType: 'exists' | 'existsAndRunning') => boolean;
}

export interface ContinueActivityDetails {
  /**
   * A string identifying the URL of the webpage accessed by the activity on another
   * device, if available.
   */
  webpageURL?: string;
}

/**
 * The object returned by `getApplicationInfoForProtocol`.
 */
export interface ApplicationInfoForProtocolReturnValue {
  /**
   * the display icon of the app handling the protocol.
   */
  icon: NativeImage;
  /**
   * installation path of the app handling the protocol.
   */
  path: string;
  /**
   * display name of the app handling the protocol.
   */
  name: string;
}

export interface FileIconOptions {
  size?: 'small' | 'normal' | 'large';
}

export interface LaunchItems {
  /**
   * name value of a registry entry.
   *
   * @platform win32
   */
  name: string;
  /**
   * The executable to an app that corresponds to a registry entry.
   *
   * @platform win32
   */
  path: string;
  /**
   * the command-line arguments to pass to the executable.
   *
   * @platform win32
   */
  args: string[];
  /**
   * one of `user` or `machine`. Indicates whether the registry entry is under
   * `HKEY_CURRENT USER` or `HKEY_LOCAL_MACHINE`.
   *
   * @platform win32
   */
  scope: string;
  /**
   * `true` if the app registry key is startup approved and therefore shows as
   * `enabled` in Task Manager and Windows settings.
   *
   * @platform win32
   */
  enabled: boolean;
}

export interface LoginItemSettings {
  /**
   * `true` if the app is set to open at login.
   */
  openAtLogin: boolean;
  /**
   * `true` if the app is set to open as hidden at login. This does not work on macOS
   * 13 and up.
   *
   * @deprecated
   * @platform darwin
   */
  openAsHidden: boolean;
  /**
   * `true` if the app was opened at login automatically.
   *
   * @platform darwin
   */
  wasOpenedAtLogin: boolean;
  /**
   * `true` if the app was opened as a hidden login item. This indicates that the app
   * should not open any windows at startup. This setting is not available on MAS
   * builds or on macOS 13 and up.
   *
   * @deprecated
   * @platform darwin
   */
  wasOpenedAsHidden: boolean;
  /**
   * `true` if the app was opened as a login item that should restore the state from
   * the previous session. This indicates that the app should restore the windows
   * that were open the last time the app was closed. This setting is not available
   * on MAS builds or on macOS 13 and up.
   *
   * @deprecated
   * @platform darwin
   */
  restoreState: boolean;
  /**
   * can be one of `not-registered`, `enabled`, `requires-approval`, or `not-found`.
   *
   * @platform darwin
   */
  status: string;
  /**
   * `true` if app is set to open at login and its run key is not deactivated. This
   * differs from `openAtLogin` as it ignores the `args` option, this property will
   * be true if the given executable would be launched at login with **any**
   * arguments.
   *
   * @platform win32
   */
  executableWillLaunchAtLogin: boolean;
  launchItems: LaunchItems[];
}

export interface AboutPanelOptions {
  /**
   * The app's name.
   */
  applicationName?: string;
  /**
   * The app's version.
   */
  applicationVersion?: string;
  /**
   * Copyright information.
   */
  copyright?: string;
  /**
   * The app's build version number.
   *
   * @platform darwin
   */
  version?: string;
  /**
   * Credit information.
   *
   * @platform darwin,win32
   */
  credits?: string;
  /**
   * List of app authors.
   *
   * @platform linux
   */
  authors?: string[];
  /**
   * The app's website.
   *
   * @platform linux
   */
  website?: string;
  /**
   * Path to the app's icon in a JPEG or PNG file format. On Linux, will be shown as
   * 64x64 pixels while retaining aspect ratio. On Windows, a 48x48 PNG will result
   * in the best visual quality.
   *
   * @platform linux,win32
   */
  iconPath?: string;
}

export interface LoginItemSettingsOptions {
  /**
   * Can be one of `mainAppService`, `agentService`, `daemonService`, or
   * `loginItemService`. Defaults to `mainAppService`. Only available on macOS 13 and
   * up. See app.setLoginItemSettings for more information about each type.
   *
   * @platform darwin
   */
  type?: string;
  /**
   * The name of the service. Required if `type` is non-default. Only available on
   * macOS 13 and up.
   *
   * @platform darwin
   */
  serviceName?: string;
  /**
   * The executable path to compare against. Defaults to `process.execPath`.
   *
   * @platform win32
   */
  path?: string;
  /**
   * The command-line arguments to compare against. Defaults to an empty array.
   *
   * @platform win32
   */
  args?: string[];
}

export interface RelaunchOptions {
  args?: string[];
  execPath?: string;
}

export interface Settings {
  /**
   * `true` to open the app at login, `false` to remove the app as a login item.
   * Defaults to `false`.
   */
  openAtLogin?: boolean;
  /**
   * `true` to open the app as hidden. Defaults to `false`. The user can edit this
   * setting from the System Preferences so
   * `app.getLoginItemSettings().wasOpenedAsHidden` should be checked when the app is
   * opened to know the current value. This setting is not available on MAS builds or
   * on macOS 13 and up.
   *
   * @deprecated
   * @platform darwin
   */
  openAsHidden?: boolean;
  /**
   * The type of service to add as a login item. Defaults to `mainAppService`. Only
   * available on macOS 13 and up.
   *
   * @platform darwin
   */
  type?:
    | 'mainAppService'
    | 'agentService'
    | 'daemonService'
    | 'loginItemService';
  /**
   * The name of the service. Required if `type` is non-default. Only available on
   * macOS 13 and up.
   *
   * @platform darwin
   */
  serviceName?: string;
  /**
   * The executable to launch at login. Defaults to `process.execPath`.
   *
   * @platform win32
   */
  path?: string;
  /**
   * The command-line arguments to pass to the executable. Defaults to an empty
   * array. Take care to wrap paths in quotes.
   *
   * @platform win32
   */
  args?: string[];
  /**
   * `true` will change the startup approved registry key and `enable / disable` the
   * App in Task Manager and Windows Settings. Defaults to `true`.
   *
   * @platform win32
   */
  enabled?: boolean;
  /**
   * value name to write into registry. Defaults to the app's AppUserModelId().
   *
   * @platform win32
   */
  name?: string;
}

/**
 * Control your application's event lifecycle.
 * @public
 */
export interface App extends EventEmitter {
  setVersion(version: string): void;
  setDesktopName(name: string): void;
  setAppPath(path: string | null): void;
  /**
   * Emitted when the application is activated. Various actions can trigger this
   * event, such as launching the application for the first time, attempting to
   * re-launch the application when it's already running, or clicking on the
   * application's dock or taskbar icon.
   *
   * @platform darwin
   */
  on(
    event: 'activate',
    listener: (event: Event, hasVisibleWindows: boolean) => void
  ): this;
  /**
   * @platform darwin
   */
  off(
    event: 'activate',
    listener: (event: Event, hasVisibleWindows: boolean) => void
  ): this;
  /**
   * @platform darwin
   */
  once(
    event: 'activate',
    listener: (event: Event, hasVisibleWindows: boolean) => void
  ): this;
  /**
   * @platform darwin
   */
  addListener(
    event: 'activate',
    listener: (event: Event, hasVisibleWindows: boolean) => void
  ): this;
  /**
   * @platform darwin
   */
  removeListener(
    event: 'activate',
    listener: (event: Event, hasVisibleWindows: boolean) => void
  ): this;
  /**
   * Emitted during Handoff after an activity from this device was successfully
   * resumed on another one.
   *
   * @platform darwin
   */
  on(
    event: 'activity-was-continued',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity.
       */
      userInfo: unknown
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  off(
    event: 'activity-was-continued',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity.
       */
      userInfo: unknown
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  once(
    event: 'activity-was-continued',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity.
       */
      userInfo: unknown
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  addListener(
    event: 'activity-was-continued',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity.
       */
      userInfo: unknown
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  removeListener(
    event: 'activity-was-continued',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity.
       */
      userInfo: unknown
    ) => void
  ): this;
  /**
   * Emitted before the application starts closing its windows. Calling
   * `event.preventDefault()` will prevent the default behavior, which is terminating
   * the application.
   *
   * [!NOTE] On Windows, this event will not be emitted if the app is closed due to
   * a shutdown/restart of the system or a user logout.
   */
  on(event: 'before-quit', listener: (event: Event) => void): this;
  off(event: 'before-quit', listener: (event: Event) => void): this;
  once(event: 'before-quit', listener: (event: Event) => void): this;
  addListener(event: 'before-quit', listener: (event: Event) => void): this;
  removeListener(event: 'before-quit', listener: (event: Event) => void): this;

  /**
   * Emitted when a LynxWindow gets blurred.
   */
  on(
    event: 'lynx-window-blur',
    listener: (event: Event, window: LynxWindow) => void
  ): this;
  off(
    event: 'lynx-window-blur',
    listener: (event: Event, window: LynxWindow) => void
  ): this;
  once(
    event: 'lynx-window-blur',
    listener: (event: Event, window: LynxWindow) => void
  ): this;
  addListener(
    event: 'lynx-window-blur',
    listener: (event: Event, window: LynxWindow) => void
  ): this;
  removeListener(
    event: 'lynx-window-blur',
    listener: (event: Event, window: LynxWindow) => void
  ): this;

  /**
   * Emitted when a new LynxWindow is created.
   */
  on(
    event: 'lynx-window-created',
    listener: (event: Event, window: LynxWindow) => void
  ): this;
  off(
    event: 'lynx-window-created',
    listener: (event: Event, window: LynxWindow) => void
  ): this;
  once(
    event: 'lynx-window-created',
    listener: (event: Event, window: LynxWindow) => void
  ): this;
  addListener(
    event: 'lynx-window-created',
    listener: (event: Event, window: LynxWindow) => void
  ): this;
  removeListener(
    event: 'lynx-window-created',
    listener: (event: Event, window: LynxWindow) => void
  ): this;

  /**
   * Emitted when a LynxWindow gets focused.
   */
  on(
    event: 'lynx-window-focus',
    listener: (event: Event, window: LynxWindow) => void
  ): this;
  off(
    event: 'lynx-window-focus',
    listener: (event: Event, window: LynxWindow) => void
  ): this;
  once(
    event: 'lynx-window-focus',
    listener: (event: Event, window: LynxWindow) => void
  ): this;
  addListener(
    event: 'lynx-window-focus',
    listener: (event: Event, window: LynxWindow) => void
  ): this;
  removeListener(
    event: 'lynx-window-focus',
    listener: (event: Event, window: LynxWindow) => void
  ): this;

  /**
   * Emitted during Handoff when an activity from a different device wants to be
   * resumed. You should call `event.preventDefault()` if you want to handle this
   * event.
   *
   * A user activity can be continued only in an app that has the same developer Team
   * ID as the activity's source app and that supports the activity's type. Supported
   * activity types are specified in the app's `Info.plist` under the
   * `NSUserActivityTypes` key.
   *
   * @platform darwin
   */
  on(
    event: 'continue-activity',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity on another device.
       */
      userInfo: unknown,
      details: ContinueActivityDetails
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  off(
    event: 'continue-activity',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity on another device.
       */
      userInfo: unknown,
      details: ContinueActivityDetails
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  once(
    event: 'continue-activity',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity on another device.
       */
      userInfo: unknown,
      details: ContinueActivityDetails
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  addListener(
    event: 'continue-activity',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity on another device.
       */
      userInfo: unknown,
      details: ContinueActivityDetails
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  removeListener(
    event: 'continue-activity',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity on another device.
       */
      userInfo: unknown,
      details: ContinueActivityDetails
    ) => void
  ): this;
  /**
   * Emitted during Handoff when an activity from a different device fails to be
   * resumed.
   *
   * @platform darwin
   */
  on(
    event: 'continue-activity-error',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * A string with the error's localized description.
       */
      error: string
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  off(
    event: 'continue-activity-error',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * A string with the error's localized description.
       */
      error: string
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  once(
    event: 'continue-activity-error',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * A string with the error's localized description.
       */
      error: string
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  addListener(
    event: 'continue-activity-error',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * A string with the error's localized description.
       */
      error: string
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  removeListener(
    event: 'continue-activity-error',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * A string with the error's localized description.
       */
      error: string
    ) => void
  ): this;
  /**
   * Emitted when the application becomes active. This differs from the `activate`
   * event in that `did-become-active` is emitted every time the app becomes active,
   * not only when Dock icon is clicked or application is re-launched. It is also
   * emitted when a user switches to the app via the macOS App Switcher.
   *
   * @platform darwin
   */
  on(event: 'did-become-active', listener: (event: Event) => void): this;
  /**
   * @platform darwin
   */
  off(event: 'did-become-active', listener: (event: Event) => void): this;
  /**
   * @platform darwin
   */
  once(event: 'did-become-active', listener: (event: Event) => void): this;
  /**
   * @platform darwin
   */
  addListener(
    event: 'did-become-active',
    listener: (event: Event) => void
  ): this;
  /**
   * @platform darwin
   */
  removeListener(
    event: 'did-become-active',
    listener: (event: Event) => void
  ): this;
  /**
   * Emitted when the app is no longer active and doesn’t have focus. This can be
   * triggered, for example, by clicking on another application or by using the macOS
   * App Switcher to switch to another application.
   *
   * @platform darwin
   */
  on(event: 'did-resign-active', listener: (event: Event) => void): this;
  /**
   * @platform darwin
   */
  off(event: 'did-resign-active', listener: (event: Event) => void): this;
  /**
   * @platform darwin
   */
  once(event: 'did-resign-active', listener: (event: Event) => void): this;
  /**
   * @platform darwin
   */
  addListener(
    event: 'did-resign-active',
    listener: (event: Event) => void
  ): this;
  /**
   * @platform darwin
   */
  removeListener(
    event: 'did-resign-active',
    listener: (event: Event) => void
  ): this;

  /**
   * Emitted when the user clicks the native macOS new tab button. The new tab button
   * is only visible if the current `LynxWindow` has a `tabbingIdentifier`
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
   * Emitted when the user wants to open a file with the application. The `open-file`
   * event is usually emitted when the application is already open and the OS wants
   * to reuse the application to open the file. `open-file` is also emitted when a
   * file is dropped onto the dock and the application is not yet running. Make sure
   * to listen for the `open-file` event very early in your application startup to
   * handle this case (even before the `ready` event is emitted).
   *
   * You should call `event.preventDefault()` if you want to handle this event.
   *
   * On Windows, you have to parse `process.argv` (in the main process) to get the
   * filepath.
   *
   * @platform darwin
   */
  on(event: 'open-file', listener: (event: Event, path: string) => void): this;
  /**
   * @platform darwin
   */
  off(event: 'open-file', listener: (event: Event, path: string) => void): this;
  /**
   * @platform darwin
   */
  once(
    event: 'open-file',
    listener: (event: Event, path: string) => void
  ): this;
  /**
   * @platform darwin
   */
  addListener(
    event: 'open-file',
    listener: (event: Event, path: string) => void
  ): this;
  /**
   * @platform darwin
   */
  removeListener(
    event: 'open-file',
    listener: (event: Event, path: string) => void
  ): this;
  /**
   * Emitted when the user wants to open a URL with the application. Your
   * application's `Info.plist` file must define the URL scheme within the
   * `CFBundleURLTypes` key, and set `NSPrincipalClass` to
   * `LynxtronApplication`.
   *
   * As with the `open-file` event, be sure to register a listener for the `open-url`
   * event early in your application startup to detect if the application is being
   * opened to handle a URL. If you register the listener in response to a `ready`
   * event, you'll miss URLs that trigger the launch of your application.
   *
   * @platform darwin
   */
  on(event: 'open-url', listener: (event: Event, url: string) => void): this;
  /**
   * @platform darwin
   */
  off(event: 'open-url', listener: (event: Event, url: string) => void): this;
  /**
   * @platform darwin
   */
  once(event: 'open-url', listener: (event: Event, url: string) => void): this;
  /**
   * @platform darwin
   */
  addListener(
    event: 'open-url',
    listener: (event: Event, url: string) => void
  ): this;
  /**
   * @platform darwin
   */
  removeListener(
    event: 'open-url',
    listener: (event: Event, url: string) => void
  ): this;
  /**
   * Emitted when the application is quitting.
   *
   * [!NOTE] On Windows, this event will not be emitted if the app is closed due to
   * a shutdown/restart of the system or a user logout.
   */
  on(event: 'quit', listener: (event: Event, exitCode: number) => void): this;
  off(event: 'quit', listener: (event: Event, exitCode: number) => void): this;
  once(event: 'quit', listener: (event: Event, exitCode: number) => void): this;
  addListener(
    event: 'quit',
    listener: (event: Event, exitCode: number) => void
  ): this;
  removeListener(
    event: 'quit',
    listener: (event: Event, exitCode: number) => void
  ): this;
  /**
   * Emitted once, when Lynxtron has finished initializing. On macOS, `launchInfo`
   * holds the `userInfo` of the `NSUserNotification` or information from
   * `UNNotificationResponse` that was used to open the application, if it was
   * launched from Notification Center. You can also call `app.isReady()` to check if
   * this event has already fired and `app.whenReady()` to get a Promise that is
   * fulfilled when Lynxtron is initialized.
   */
  on(
    event: 'ready',
    listener: (
      event: Event,
      /**
       * @platform darwin
       */
      launchInfo: Record<string, any> | NotificationResponse
    ) => void
  ): this;
  off(
    event: 'ready',
    listener: (
      event: Event,
      /**
       * @platform darwin
       */
      launchInfo: Record<string, any> | NotificationResponse
    ) => void
  ): this;
  once(
    event: 'ready',
    listener: (
      event: Event,
      /**
       * @platform darwin
       */
      launchInfo: Record<string, any> | NotificationResponse
    ) => void
  ): this;
  addListener(
    event: 'ready',
    listener: (
      event: Event,
      /**
       * @platform darwin
       */
      launchInfo: Record<string, any> | NotificationResponse
    ) => void
  ): this;
  removeListener(
    event: 'ready',
    listener: (
      event: Event,
      /**
       * @platform darwin
       */
      launchInfo: Record<string, any> | NotificationResponse
    ) => void
  ): this;

  /**
   * This event will be emitted inside the primary instance of your application when
   * a second instance has been executed and calls `app.requestSingleInstanceLock()`.
   *
   * `argv` is an Array of the second instance's command line arguments, and
   * `workingDirectory` is its current working directory. Usually applications
   * respond to this by making their primary window focused and non-minimized.
   *
   * [!NOTE] `argv` will not be exactly the same list of arguments as those passed
   * to the second instance. The order might change and additional arguments might be
   * appended. If you need to maintain the exact same arguments, it's advised to use
   * `additionalData` instead.
   *
   * [!NOTE] If the second instance is started by a different user than the first,
   * the `argv` array will not include the arguments.
   *
   * This event is guaranteed to be emitted after the `ready` event of `app` gets
   * emitted.
   */
  on(
    event: 'second-instance',
    listener: (
      event: Event,
      /**
       * An array of the second instance's command line arguments
       */
      argv: string[],
      /**
       * The second instance's working directory
       */
      workingDirectory: string,
      /**
       * A JSON object of additional data passed from the second instance
       */
      additionalData: unknown
    ) => void
  ): this;
  off(
    event: 'second-instance',
    listener: (
      event: Event,
      /**
       * An array of the second instance's command line arguments
       */
      argv: string[],
      /**
       * The second instance's working directory
       */
      workingDirectory: string,
      /**
       * A JSON object of additional data passed from the second instance
       */
      additionalData: unknown
    ) => void
  ): this;
  once(
    event: 'second-instance',
    listener: (
      event: Event,
      /**
       * An array of the second instance's command line arguments
       */
      argv: string[],
      /**
       * The second instance's working directory
       */
      workingDirectory: string,
      /**
       * A JSON object of additional data passed from the second instance
       */
      additionalData: unknown
    ) => void
  ): this;
  addListener(
    event: 'second-instance',
    listener: (
      event: Event,
      /**
       * An array of the second instance's command line arguments
       */
      argv: string[],
      /**
       * The second instance's working directory
       */
      workingDirectory: string,
      /**
       * A JSON object of additional data passed from the second instance
       */
      additionalData: unknown
    ) => void
  ): this;
  removeListener(
    event: 'second-instance',
    listener: (
      event: Event,
      /**
       * An array of the second instance's command line arguments
       */
      argv: string[],
      /**
       * The second instance's working directory
       */
      workingDirectory: string,
      /**
       * A JSON object of additional data passed from the second instance
       */
      additionalData: unknown
    ) => void
  ): this;

  /**
   * Emitted when Handoff is about to be resumed on another device. If you need to
   * update the state to be transferred, you should call `event.preventDefault()`
   * immediately, construct a new `userInfo` dictionary and call
   * `app.updateCurrentActivity()` in a timely manner. Otherwise, the operation will
   * fail and `continue-activity-error` will be called.
   *
   * @platform darwin
   */
  on(
    event: 'update-activity-state',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity.
       */
      userInfo: unknown
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  off(
    event: 'update-activity-state',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity.
       */
      userInfo: unknown
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  once(
    event: 'update-activity-state',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity.
       */
      userInfo: unknown
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  addListener(
    event: 'update-activity-state',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity.
       */
      userInfo: unknown
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  removeListener(
    event: 'update-activity-state',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string,
      /**
       * Contains app-specific state stored by the activity.
       */
      userInfo: unknown
    ) => void
  ): this;

  /**
   * Emitted during Handoff before an activity from a different device wants to be
   * resumed. You should call `event.preventDefault()` if you want to handle this
   * event.
   *
   * @platform darwin
   */
  on(
    event: 'will-continue-activity',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  off(
    event: 'will-continue-activity',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  once(
    event: 'will-continue-activity',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  addListener(
    event: 'will-continue-activity',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string
    ) => void
  ): this;
  /**
   * @platform darwin
   */
  removeListener(
    event: 'will-continue-activity',
    listener: (
      event: Event,
      /**
       * A string identifying the activity. Maps to `NSUserActivity.activityType`.
       */
      type: string
    ) => void
  ): this;
  /**
   * Emitted when the application has finished basic startup. On Windows,
   * the `will-finish-launching` event is the same as the `ready` event; on macOS,
   * this event represents the `applicationWillFinishLaunching` notification of
   * `NSApplication`.
   *
   * In most cases, you should do everything in the `ready` event handler.
   */
  on(event: 'will-finish-launching', listener: () => void): this;
  off(event: 'will-finish-launching', listener: () => void): this;
  once(event: 'will-finish-launching', listener: () => void): this;
  addListener(event: 'will-finish-launching', listener: () => void): this;
  removeListener(event: 'will-finish-launching', listener: () => void): this;
  /**
   * Emitted when all windows have been closed and the application will quit. Calling
   * `event.preventDefault()` will prevent the default behavior, which is terminating
   * the application.
   *
   * See the description of the `window-all-closed` event for the differences between
   * the `will-quit` and `window-all-closed` events.
   *
   * [!NOTE] On Windows, this event will not be emitted if the app is closed due to
   * a shutdown/restart of the system or a user logout.
   */
  on(event: 'will-quit', listener: (event: Event) => void): this;
  off(event: 'will-quit', listener: (event: Event) => void): this;
  once(event: 'will-quit', listener: (event: Event) => void): this;
  addListener(event: 'will-quit', listener: (event: Event) => void): this;
  removeListener(event: 'will-quit', listener: (event: Event) => void): this;
  /**
   * Emitted when all windows have been closed.
   *
   * If you do not subscribe to this event and all windows are closed, the default
   * behavior is to quit the app; however, if you subscribe, you control whether the
   * app quits or not. If the user pressed `Cmd + Q`, or the developer called
   * `app.quit()`, Lynxtron will first try to close all the windows and then emit the
   * `will-quit` event, and in this case the `window-all-closed` event would not be
   * emitted.
   */
  on(event: 'window-all-closed', listener: () => void): this;
  off(event: 'window-all-closed', listener: () => void): this;
  once(event: 'window-all-closed', listener: () => void): this;
  addListener(event: 'window-all-closed', listener: () => void): this;
  removeListener(event: 'window-all-closed', listener: () => void): this;
  /**
   * Adds `path` to the recent documents list.
   *
   * This list is managed by the OS. On Windows, you can visit the list from the task
   * bar, and on macOS, you can visit it from dock menu.
   *
   * @platform darwin,win32
   */
  addRecentDocument(path: string): void;
  /**
   * Clears the recent documents list.
   *
   * @platform darwin,win32
   */
  clearRecentDocuments(): void;
  /**
   * Exits immediately with `exitCode`. `exitCode` defaults to 0.
   *
   * All windows will be closed immediately without asking the user, and the
   * `before-quit` and `will-quit` events will not be emitted.
   */
  exit(exitCode?: number): void;
  /**
   * On Linux, focuses on the first visible window. On macOS, makes the application
   * the active app. On Windows, focuses on the application's first window.
   *
   * You should seek to use the `steal` option as sparingly as possible.
   */
  focus(options?: FocusOptions): void;
  /**
   * Resolve with an object containing the following:
   *
   * * `icon` NativeImage - the display icon of the app handling the protocol.
   * * `path` string  - installation path of the app handling the protocol.
   * * `name` string - display name of the app handling the protocol.
   *
   * This method returns a promise that contains the application name, icon and path
   * of the default handler for the protocol (aka URI scheme) of a URL.
   *
   * @platform darwin,win32
   */
  getApplicationInfoForProtocol(
    url: string
  ): Promise<ApplicationInfoForProtocolReturnValue>;
  /**
   * Name of the application handling the protocol, or an empty string if there is no
   * handler. For instance, if Lynxtron is the default handler of the URL, this could
   * be `Lynxtron` on Windows and Mac. However, don't rely on the precise format
   * which is not guaranteed to remain unchanged. Expect a different format on Linux,
   * possibly with a `.desktop` suffix.
   *
   * This method returns the application name of the default handler for the protocol
   * (aka URI scheme) of a URL.
   */
  getApplicationNameForProtocol(url: string): string;
  /**
   * Array of `ProcessMetric` objects that correspond to memory and CPU usage
   * statistics of all the processes associated with the app.
   */
  getAppMetrics(): ProcessMetric;
  /**
   * The current application directory.
   */
  getAppPath(): string;
  /**
   * The current value displayed in the counter badge.
   *
   * @platform linux,darwin
   */
  getBadgeCount(): number;
  /**
   * The type of the currently running activity.
   *
   * @platform darwin
   */
  getCurrentActivityType(): string;
  /**
   * * `minItems` Integer - The minimum number of items that will be shown in the
   * Jump List (for a more detailed description of this value see the MSDN docs).
   * * `removedItems` JumpListItem[] - Array of `JumpListItem` objects that
   * correspond to items that the user has explicitly removed from custom categories
   * in the Jump List. These items must not be re-added to the Jump List in the
   * **next** call to `app.setJumpList()`, Windows will not display any custom
   * category that contains any of the removed items.
   *
   * @platform win32
   */
  getJumpListSettings(): JumpListSettings;
  /**
   * The current application locale, fetched using Chromium's `l10n_util` library.
   * Possible return values are documented here.
   *
   * To set the locale, you'll want to use a command line switch at app startup,
   * which may be found here.
   *
   * > [!NOTE] When distributing your packaged app, you have to also ship the
   * `locales` folder.
   *
   * > [!NOTE] This API must be called after the `ready` event is emitted.
   *
   * > [!NOTE] To see example return values of this API compared to other locale and
   * language APIs, see `app.getPreferredSystemLanguages()`.
   */
  getLocale(): string;
  /**
   * User operating system's locale two-letter ISO 3166 country code. The value is
   * taken from native OS APIs.
   *
   * > [!NOTE] When unable to detect locale country code, it returns empty string.
   */
  getLocaleCountryCode(): string;
  /**
   * If you provided `path` and `args` options to `app.setLoginItemSettings`, then
   * you need to pass the same arguments here for `openAtLogin` to be set correctly.
   *
   *
   * * `openAtLogin` boolean - `true` if the app is set to open at login.
   * * `openAsHidden` boolean _macOS_ _Deprecated_ - `true` if the app is set to open
   * as hidden at login. This does not work on macOS 13 and up.
   * * `wasOpenedAtLogin` boolean _macOS_ - `true` if the app was opened at login
   * automatically.
   * * `wasOpenedAsHidden` boolean _macOS_ _Deprecated_ - `true` if the app was
   * opened as a hidden login item. This indicates that the app should not open any
   * windows at startup. This setting is not available on MAS builds or on macOS 13
   * and up.
   * * `restoreState` boolean _macOS_ _Deprecated_ - `true` if the app was opened as
   * a login item that should restore the state from the previous session. This
   * indicates that the app should restore the windows that were open the last time
   * the app was closed. This setting is not available on MAS builds or on macOS 13
   * and up.
   * * `status` string _macOS_ - can be one of `not-registered`, `enabled`,
   * `requires-approval`, or `not-found`.
   * * `executableWillLaunchAtLogin` boolean _Windows_ - `true` if app is set to open
   * at login and its run key is not deactivated. This differs from `openAtLogin` as
   * it ignores the `args` option, this property will be true if the given executable
   * would be launched at login with **any** arguments.
   * * `launchItems` Object[] _Windows_
   *   * `name` string _Windows_ - name value of a registry entry.
   *   * `path` string _Windows_ - The executable to an app that corresponds to a
   * registry entry.
   *   * `args` string[] _Windows_ - the command-line arguments to pass to the
   * executable.
   *   * `scope` string _Windows_ - one of `user` or `machine`. Indicates whether the
   * registry entry is under `HKEY_CURRENT USER` or `HKEY_LOCAL_MACHINE`.
   *   * `enabled` boolean _Windows_ - `true` if the app registry key is startup
   * approved and therefore shows as `enabled` in Task Manager and Windows settings.
   *
   * @platform darwin,win32
   */
  getLoginItemSettings(options?: LoginItemSettingsOptions): LoginItemSettings;
  /**
   * The current application's name, which is the name in the application's
   * `package.json` file.
   *
   * Usually the `name` field of `package.json` is a short lowercase name, according
   * to the npm modules spec. You should usually also specify a `productName` field,
   * which is your application's full capitalized name, and which will be preferred
   * over `name` by Lynxtron.
   */
  getName(): string;
  /**
   * A path to a special directory or file associated with `name`. On failure, an
   * `Error` is thrown.
   *
   * If `app.getPath('logs')` is called without called `app.setAppLogsPath()` being
   * called first, a default log directory will be created equivalent to calling
   * `app.setAppLogsPath()` without a `path` parameter.
   */
  getPath(
    name:
      | 'home'
      | 'appData'
      | 'assets'
      | 'userData'
      | 'temp'
      | 'exe'
      | 'module'
      | 'desktop'
      | 'documents'
      | 'downloads'
      | 'music'
      | 'pictures'
      | 'videos'
      | 'recent'
      | 'logs'
      | 'crashDumps'
  ): string;
  getFileIcon(path: string, options?: FileIconOptions): Promise<NativeImage>;
  /**
   * An array containing documents in the most recent documents list.
   *
   * @platform darwin,win32
   */
  getRecentDocuments(): string[];
  /**
   * The version of the loaded application. If no version is found in the
   * application's `package.json` file, the version of the current bundle or
   * executable is returned.
   */
  getVersion(): string;

  /**
   * The device model of the current device.
   */
  getDeviceModel(): string;
  /**
   * This method returns whether or not this instance of your app is currently
   * holding the single instance lock.  You can request the lock with
   * `app.requestSingleInstanceLock()` and release with
   * `app.releaseSingleInstanceLock()`
   */
  hasSingleInstanceLock(): boolean;
  /**
   * Hides all application windows without minimizing them.
   *
   * @platform darwin
   */
  hide(): void;
  /**
   * Whether the current executable is the default handler for a protocol (aka URI
   * scheme).
   *
   * > [!NOTE] On macOS, you can use this method to check if the app has been
   * registered as the default protocol handler for a protocol. You can also verify
   * this by checking `~/Library/Preferences/com.apple.LaunchServices.plist` on the
   * macOS machine. Please refer to Apple's documentation for details.
   *
   * The API uses the Windows Registry and `LSCopyDefaultHandlerForURLScheme`
   * internally.
   */
  isDefaultProtocolClient(
    protocol: string,
    path?: string,
    args?: string[]
  ): boolean;
  /**
   * whether or not the current OS version allows for native emoji pickers.
   */
  isEmojiPanelSupported(): boolean;
  /**
   * `true` if the application—including all of its windows—is hidden (e.g. with
   * `Command-H`), `false` otherwise.
   *
   * @platform darwin
   */
  isHidden(): boolean;
  /**
   * Whether the application is currently running from the systems Application
   * folder. Use in combination with `app.moveToApplicationsFolder()`
   *
   * @platform darwin
   */
  isInApplicationsFolder(): boolean;
  /**
   * `true` if Lynxtron has finished initializing, `false` otherwise. See also
   * `app.whenReady()`.
   */
  isReady(): boolean;
  /**
   * Try to close all windows. The `before-quit` event will be emitted first. If all
   * windows are successfully closed, the `will-quit` event will be emitted and by
   * default the application will terminate.
   *
   * A window may still cancel the quit during its close flow.
   */
  quit(): void;
  /**
   * Relaunches the app when the current instance exits.
   *
   * By default, the new instance will use the same working directory and command
   * line arguments as the current instance. When `args` is specified, the `args`
   * will be passed as the command line arguments instead. When `execPath` is
   * specified, the `execPath` will be executed for the relaunch instead of the
   * current app.
   *
   * Note that this method does not quit the app when executed. You have to call
   * `app.quit` or `app.exit` after calling `app.relaunch` to make the app restart.
   *
   * When `app.relaunch` is called multiple times, multiple instances will be started
   * after the current instance exits.
   *
   * An example of restarting the current instance immediately and adding a new
   * command line argument to the new instance:
   */
  relaunch(options?: RelaunchOptions): void;
  /**
   * Releases all locks that were created by `requestSingleInstanceLock`. This will
   * allow multiple instances of the application to once again run side by side.
   */
  releaseSingleInstanceLock(): void;
  /**
   * Whether the call succeeded.
   *
   * This method checks if the current executable as the default handler for a
   * protocol (aka URI scheme). If so, it will remove the app as the default handler.
   *
   * @platform darwin,win32
   */
  removeAsDefaultProtocolClient(
    protocol: string,
    path?: string,
    args?: string[]
  ): boolean;
  /**
   * The return value of this method indicates whether or not this instance of your
   * application successfully obtained the lock.  If it failed to obtain the lock,
   * you can assume that another instance of your application is already running with
   * the lock and exit immediately.
   *
   * I.e. This method returns `true` if your process is the primary instance of your
   * application and your app should continue loading.  It returns `false` if your
   * process should immediately quit as it has sent its parameters to another
   * instance that has already acquired the lock.
   *
   * On macOS, the system enforces single instance automatically when users try to
   * open a second instance of your app in Finder, and the `open-file` and `open-url`
   * events will be emitted for that. However when users start your app in command
   * line, the system's single instance mechanism will be bypassed, and you have to
   * use this method to ensure single instance.
   *
   * An example of activating the window of primary instance when a second instance
   * starts:
   */
  requestSingleInstanceLock(additionalData?: Record<any, any>): boolean;
  /**
   * Set the about panel options. This will override the values defined in the app's
   * `.plist` file on macOS. See the Apple docs for more details. On Linux, values
   * must be set in order to be shown; there are no defaults.
   *
   * If you do not set `credits` but still wish to surface them in your app, AppKit
   * will look for a file named "Credits.html", "Credits.rtf", and "Credits.rtfd", in
   * that order, in the bundle returned by the NSBundle class method main. The first
   * file found is used, and if none is found, the info area is left blank. See Apple
   * documentation for more information.
   */
  setAboutPanelOptions(options: AboutPanelOptions): void;
  /**
   * Sets the activation policy for a given app.
   *
   * Activation policy types:
   *
   * * 'regular' - The application is an ordinary app that appears in the Dock and
   * may have a user interface.
   * * 'accessory' - The application doesn’t appear in the Dock and doesn’t have a
   * menu bar, but it may be activated programmatically or by clicking on one of its
   * windows.
   * * 'prohibited' - The application doesn’t appear in the Dock and may not create
   * windows or be activated.
   *
   * @platform darwin
   */
  setActivationPolicy(policy: 'regular' | 'accessory' | 'prohibited'): void;
  /**
   * Sets or creates a directory your app's logs which can then be manipulated with
   * `app.getPath()` or `app.setPath(pathName, newPath)`.
   *
   * Calling `app.setAppLogsPath()` without a `path` parameter will result in this
   * directory being set to `~/Library/Logs/YourAppName` on _macOS_, and inside the
   * `userData` directory on _Linux_ and _Windows_.
   */
  setAppLogsPath(path?: string): void;
  /**
   * Changes the Application User Model ID to `id`.
   *
   * @platform win32
   */
  setAppUserModelId(id: string): void;
  /**
   * Whether the call succeeded.
   *
   * Sets the current executable as the default handler for a protocol (aka URI
   * scheme). It allows you to integrate your app deeper into the operating system.
   * Once registered, all links with `your-protocol://` will be opened with the
   * current executable. The whole link, including protocol, will be passed to your
   * application as a parameter.
   *
   * > [!NOTE] On macOS, you can only register protocols that have been added to your
   * app's `info.plist`, which cannot be modified at runtime. However, you can change
   * the file during build time via Lynxtron Forge, Lynxtron Packager, or by editing
   * `info.plist` with a text editor. Please refer to Apple's documentation for
   * details.
   *
   * > [!NOTE] In a Windows Store environment (when packaged as an `appx`) this API
   * will return `true` for all calls but the registry key it sets won't be
   * accessible by other applications.  In order to register your Windows Store
   * application as a default protocol handler you must declare the protocol in your
   * manifest.
   *
   * The API uses the Windows Registry and `LSSetDefaultHandlerForURLScheme`
   * internally.
   */
  setAsDefaultProtocolClient(
    protocol: string,
    path?: string,
    args?: string[]
  ): boolean;
  /**
   * Sets or removes a custom Jump List for the application, and returns one of the
   * following strings:
   *
   * * `ok` - Nothing went wrong.
   * * `error` - One or more errors occurred, enable runtime logging to figure out
   * the likely cause.
   * * `invalidSeparatorError` - An attempt was made to add a separator to a custom
   * category in the Jump List. Separators are only allowed in the standard `Tasks`
   * category.
   * * `fileTypeRegistrationError` - An attempt was made to add a file link to the
   * Jump List for a file type the app isn't registered to handle.
   * * `customCategoryAccessDeniedError` - Custom categories can't be added to the
   * Jump List due to user privacy or group policy settings.
   *
   * If `categories` is `null` the previously set custom Jump List (if any) will be
   * replaced by the standard Jump List for the app (managed by Windows).
   *
   * [!NOTE] If a `JumpListCategory` object has neither the `type` nor the `name`
   * property set then its `type` is assumed to be `tasks`. If the `name` property is
   * set but the `type` property is omitted then the `type` is assumed to be
   * `custom`.
   *
   * [!NOTE] Users can remove items from custom categories, and Windows will not
   * allow a removed item to be added back into a custom category until **after** the
   * next successful call to `app.setJumpList(categories)`. Any attempt to re-add a
   * removed item to a custom category earlier than that will result in the entire
   * custom category being omitted from the Jump List. The list of removed items can
   * be obtained using `app.getJumpListSettings()`.
   *
   * [!NOTE] The maximum length of a Jump List item's `description` property is 260
   * characters. Beyond this limit, the item will not be added to the Jump List, nor
   * will it be displayed.
   *
   * Here's a very simple example of creating a custom Jump List:
   *
   * @platform win32
   */
  setJumpList(
    categories: JumpListCategory[] | null
  ):
    | 'ok'
    | 'error'
    | 'invalidSeparatorError'
    | 'fileTypeRegistrationError'
    | 'customCategoryAccessDeniedError';
  /**
   * Set the app's login item settings.
   *
   * To work with Lynxtron's `autoUpdater` on Windows, which uses Squirrel, you'll
   * want to set the launch path to your executable's name but a directory up, which
   * is a stub application automatically generated by Squirrel which will
   * automatically launch the latest version.
   *
   * For more information about setting different services as login items on macOS 13
   * and up, see `SMAppService`.
   *
   * @platform darwin,win32
   */
  setLoginItemSettings(settings: Settings): void;
  /**
   * Overrides the current application's name.
   *
   * > [!NOTE] This function overrides the name used internally by Lynxtron; it does
   * not affect the name that the OS uses.
   */
  setName(name: string): void;
  /**
   * Overrides the `path` to a special directory or file associated with `name`. If
   * the path specifies a directory that does not exist, an `Error` is thrown. In
   * that case, the directory should be created with `fs.mkdirSync` or similar.
   *
   * You can only override paths of a `name` defined in `app.getPath`. Override the
   * path before the `ready` event if the runtime should use the custom location
   * during startup.
   */
  setPath(name: string, path: string): void;
  /**
   * Creates an `NSUserActivity` and sets it as the current activity. The activity is
   * eligible for Handoff to another device afterward.
   *
   * @platform darwin
   */
  setUserActivity(type: string, userInfo: any, webpageURL?: string): void;
  /**
   * Adds `tasks` to the Tasks category of the Jump List on Windows.
   *
   * `tasks` is an array of `Task` objects.
   *
   * Whether the call succeeded.
   *
   * > [!NOTE] If you'd like to customize the Jump List even more use
   * `app.setJumpList(categories)` instead.
   *
   * @platform win32
   */
  setUserTasks(tasks: Task[]): boolean;
  /**
   * Shows application windows after they were hidden. Does not automatically focus
   * them.
   *
   * @platform darwin
   */
  show(): void;
  /**
   * Show the app's about panel options. These options can be overridden with
   * `app.setAboutPanelOptions(options)`. This function runs asynchronously.
   */
  showAboutPanel(): void;
  /**
   * This function **must** be called once you have finished accessing the security
   * scoped file. If you do not remember to stop accessing the bookmark, kernel
   * resources will be leaked and your app will lose its ability to reach outside the
   * sandbox completely, until your app is restarted.
   *
   * Start accessing a security scoped resource. With this method Lynxtron
   * applications that are packaged for the Mac App Store may reach outside their
   * sandbox to access files chosen by the user. See Apple's documentation for a
   * description of how this system works.
   *
   * @platform mas
   */
  startAccessingSecurityScopedResource(bookmarkData: string): Function;
  /**
   * fulfilled when Lynxtron is initialized. May be used as a convenient alternative
   * to checking `app.isReady()` and subscribing to the `ready` event if the app is
   * not ready yet.
   */
  whenReady(): Promise<void>;
  /**
   * A `Menu | null` property that returns `Menu` if one has been set and `null`
   * otherwise. Users can pass a Menu to set this property.
   */
  applicationMenu: Menu | null;
  /**
   * An `Integer` property that returns the badge count for current app. Setting the
   * count to `0` will hide the badge.
   *
   * On macOS, setting this with any nonzero integer shows on the dock icon. On
   * Linux, this property only works for Unity launcher.
   *
   * > [!NOTE] Unity launcher requires a `.desktop` file to work. For more
   * information, please read the Unity integration documentation.
   *
   * > [!NOTE] On macOS, you need to ensure that your application has the permission
   * to display notifications for this property to take effect.
   *
   * @platform linux,darwin
   */
  badgeCount: number;
  /**
   * A `CommandLine` object that allows you to read and manipulate the command line
   * arguments.
   *
   */
  readonly commandLine: CommandLine;
  /**
   * A `Dock | undefined` property (`Dock` on macOS, `undefined` on all other
   * platforms) that allows you to perform actions on your app icon in the user's
   * dock.
   *
   * @platform darwin
   */
  readonly dock: Dock | undefined;
  /**
   * A `boolean` property that returns  `true` if the app is packaged, `false`
   * otherwise. For many apps, this property can be used to distinguish development
   * and production environments.
   *
   */
  readonly isPackaged: boolean;
  /**
   * A `string` property that indicates the current application's name, which is the
   * name in the application's `package.json` file.
   *
   * Usually the `name` field of `package.json` is a short lowercase name, according
   * to the npm modules spec. You should usually also specify a `productName` field,
   * which is your application's full capitalized name, and which will be preferred
   * over `name` by Lynxtron.
   */
  name: string;
  /**
   * A `boolean` which when `true` indicates that the app is currently running under
   * an ARM64 translator (like the macOS Rosetta Translator Environment or Windows
   * WOW).
   *
   * You can use this property to prompt users to download the arm64 version of your
   * application when they are mistakenly running the x64 version under Rosetta or
   * WOW.
   *
   * @platform darwin,win32
   */
  readonly runningUnderARM64Translation: boolean;
}

export declare const app: App;
