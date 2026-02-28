import { BaseWindow, Event } from 'lynxtron';
import type { LynxWindow as LWT } from 'lynxtron';
const { LynxWindow } = process._linkedBinding('lynxtron_lynx_window') as {
  LynxWindow: typeof LWT;
};

Object.setPrototypeOf(LynxWindow.prototype, BaseWindow.prototype);

LynxWindow.prototype._init = function (this: LWT) {
  // Call parent class's _init.
  BaseWindow.prototype._init.call(this);

  // Avoid recursive require.
  const { app } = require('lynxtron');

  // Set ID at constructon time so it's accessible after
  // underlying window destruction.
  const id = this.id;
  Object.defineProperty(this, 'id', {
    value: id,
    writable: false,
  });

  const nativeSetBounds = this.setBounds;
  this.setBounds = (bounds, ...opts) => {
    bounds = {
      ...this.getBounds(),
      ...bounds,
    };
    nativeSetBounds.call(this, bounds, ...opts);
  };

  // Dispatch messages from lynx window to the LynxBridgeMain module.
  // this.on('-lynx-message', function (this: LWT, event, channel, args) {
  //   lynxBridgeMain.emit(channel, event, args);
  // });

  // this.on('-lynx-invoke', function (this: LWT, event, channel, args) {
  //   // event.sender = event.sender || this;
  //   lynxBridgeMain.emit('-internal-lynx-invoke', event, channel, args);
  // });

  // this.on('lynx-file-load', function (this: LWT, event, args) {
  //   lynxBridgeMain.emit('lynx-file-load', event, args);
  // });

  // this.on('lynx-file-verify-complete', function (this: LWT, event, args) {
  //   lynxBridgeMain.emit('lynx-file-verify-complete', event, args);
  // });

  // Redirect focus/blur event to app instance too.
  this.on('blur', (event: Event) => {
    app.emit('lynx-window-blur', event, this);
  });
  this.on('focus', (event: Event) => {
    app.emit('lynx-window-focus', event, this);
  });

  // Subscribe to visibilityState changes and pass to renderer process.
  let isVisible = this.isVisible() && !this.isMinimized();
  const visibilityChanged = () => {
    const newState = this.isVisible() && !this.isMinimized();
    if (isVisible !== newState) {
      isVisible = newState;
      // const visibilityState = isVisible ? 'visible' : 'hidden';
      // this.webContents.emit('-window-visibility-change', visibilityState);
    }
  };

  const visibilityEvents = ['show', 'hide', 'minimize', 'maximize', 'restore'];
  for (const event of visibilityEvents) {
    this.on(event as any, visibilityChanged);
  }

  app.emit('lynx-window-created', { preventDefault() {} }, this);

  // Object.defineProperty(this, 'devToolsWebContents', {
  //   enumerable: true,
  //   configurable: false,
  //   get () {
  //     return this.webContents.devToolsWebContents;
  //   }
  // });
};

const isLynxWindow = (win: any) => {
  return win && win.constructor.name === 'LynxWindow';
};

LynxWindow.fromId = (id: number) => {
  const win = BaseWindow.fromId(id);
  return isLynxWindow(win) ? ((win as any) as LWT) : null;
};

LynxWindow.getAllWindows = () => {
  return (BaseWindow.getAllWindows().filter(isLynxWindow) as any[]) as LWT[];
};

LynxWindow.getFocusedWindow = () => {
  for (const window of LynxWindow.getAllWindows()) {
    if (window.isFocused()) return window;
  }
  return null;
};

// LynxWindow.fromWebContents = (webContents: WebContents) => {
//   return webContents.getOwnerBrowserWindow();
// };

// LynxWindow.fromBrowserView = (browserView: BrowserView) => {
//   return LynxWindow.fromWebContents(browserView.webContents);
// };

// LynxWindow.prototype.setTouchBar = function (touchBar) {
//   (TouchBar as any)._setOnWindow(touchBar, this);
// };

// Forwarded to webContents:

// LynxWindow.prototype.loadURL = function (...args) {
//   return this.webContents.loadURL(...args);
// };

// LynxWindow.prototype.getURL = function () {
//   return this.webContents.getURL();
// };

// LynxWindow.prototype.loadFile = function (...args) {
//   return this.webContents.loadFile(...args);
// };

// LynxWindow.prototype.reload = function (...args) {
//   return this.webContents.reload(...args);
// };

// LynxWindow.prototype.send = function (...args) {
//   return this.webContents.send(...args);
// };

// LynxWindow.prototype.openDevTools = function (...args) {
//   return this.webContents.openDevTools(...args);
// };

// LynxWindow.prototype.closeDevTools = function () {
//   return this.webContents.closeDevTools();
// };

// LynxWindow.prototype.isDevToolsOpened = function () {
//   return this.webContents.isDevToolsOpened();
// };

// LynxWindow.prototype.isDevToolsFocused = function () {
//   return this.webContents.isDevToolsFocused();
// };

// LynxWindow.prototype.toggleDevTools = function () {
//   return this.webContents.toggleDevTools();
// };

// LynxWindow.prototype.inspectElement = function (...args) {
//   return this.webContents.inspectElement(...args);
// };

// LynxWindow.prototype.inspectSharedWorker = function () {
//   return this.webContents.inspectSharedWorker();
// };

// LynxWindow.prototype.inspectServiceWorker = function () {
//   return this.webContents.inspectServiceWorker();
// };

// LynxWindow.prototype.showDefinitionForSelection = function () {
//   return this.webContents.showDefinitionForSelection();
// };

// LynxWindow.prototype.capturePage = function (...args) {
//   return this.webContents.capturePage(...args);
// };

// LynxWindow.prototype.getBackgroundThrottling = function () {
//   return this.webContents.getBackgroundThrottling();
// };

// LynxWindow.prototype.setBackgroundThrottling = function (allowed: boolean) {
//   return this.webContents.setBackgroundThrottling(allowed);
// };

module.exports = LynxWindow;
