// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { EventEmitter } from 'node:events';

export interface PowerMonitor extends EventEmitter {
  // Docs: https://electronjs.org/docs/api/power-monitor

  /**
   * Emitted when the system is about to lock the screen.
   *
   * @platform darwin,win32
   */
  on(event: 'lock-screen', listener: Function): this;
  once(event: 'lock-screen', listener: Function): this;
  addListener(event: 'lock-screen', listener: Function): this;
  removeListener(event: 'lock-screen', listener: Function): this;
  /**
   * Emitted when the system changes to AC power.
   *
   * @platform darwin,win32
   */
  on(event: 'on-ac', listener: Function): this;
  once(event: 'on-ac', listener: Function): this;
  addListener(event: 'on-ac', listener: Function): this;
  removeListener(event: 'on-ac', listener: Function): this;
  /**
   * Emitted when system changes to battery power.
   *
   * @platform darwin
   */
  on(event: 'on-battery', listener: Function): this;
  once(event: 'on-battery', listener: Function): this;
  addListener(event: 'on-battery', listener: Function): this;
  removeListener(event: 'on-battery', listener: Function): this;
  /**
   * Emitted when system is resuming.
   */
  on(event: 'resume', listener: Function): this;
  once(event: 'resume', listener: Function): this;
  addListener(event: 'resume', listener: Function): this;
  removeListener(event: 'resume', listener: Function): this;
  /**
   * Emitted when the system is about to reboot or shut down. If the event handler
   * invokes `e.preventDefault()`, Electron will attempt to delay system shutdown in
   * order for the app to exit cleanly. If `e.preventDefault()` is called, the app
   * should exit as soon as possible by calling something like `app.quit()`.
   *
   * @platform linux,darwin
   */
  on(event: 'shutdown', listener: Function): this;
  once(event: 'shutdown', listener: Function): this;
  addListener(event: 'shutdown', listener: Function): this;
  removeListener(event: 'shutdown', listener: Function): this;
  /**
   * Emitted when the system is suspending.
   */
  on(event: 'suspend', listener: Function): this;
  once(event: 'suspend', listener: Function): this;
  addListener(event: 'suspend', listener: Function): this;
  removeListener(event: 'suspend', listener: Function): this;
  /**
   * Emitted as soon as the systems screen is unlocked.
   *
   * @platform darwin,win32
   */
  on(event: 'unlock-screen', listener: Function): this;
  once(event: 'unlock-screen', listener: Function): this;
  addListener(event: 'unlock-screen', listener: Function): this;
  removeListener(event: 'unlock-screen', listener: Function): this;
  /**
   * Emitted when a login session is activated. See documentation for more
   * information.
   *
   * @platform darwin
   */
  on(event: 'user-did-become-active', listener: Function): this;
  once(event: 'user-did-become-active', listener: Function): this;
  addListener(event: 'user-did-become-active', listener: Function): this;
  removeListener(event: 'user-did-become-active', listener: Function): this;
  /**
   * Emitted when a login session is deactivated. See documentation for more
   * information.
   *
   * @platform darwin
   */
  on(event: 'user-did-resign-active', listener: Function): this;
  once(event: 'user-did-resign-active', listener: Function): this;
  addListener(event: 'user-did-resign-active', listener: Function): this;
  removeListener(event: 'user-did-resign-active', listener: Function): this;
  /**
   * The system's current state. Can be `active`, `idle`, `locked` or `unknown`.
   *
   * Calculate the system idle state. `idleThreshold` is the amount of time (in
   * seconds) before considered idle.  `locked` is available on supported systems
   * only.
   */
  getSystemIdleState(
    idleThreshold: number
  ): 'active' | 'idle' | 'locked' | 'unknown';
  /**
   * Idle time in seconds
   *
   * Calculate system idle time in seconds.
   */
  getSystemIdleTime(): number;
  /**
   * Whether the system is on battery power.
   *
   * To monitor for changes in this property, use the `on-battery` and `on-ac`
   * events.
   */
  isOnBatteryPower(): boolean;
  /**
   * A `boolean` property. True if the system is on battery power.
   *
   * See `powerMonitor.isOnBatteryPower()`.
   */
  onBatteryPower: boolean;
}
