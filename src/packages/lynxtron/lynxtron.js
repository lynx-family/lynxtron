// @ts-nocheck

import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const lynxtron = require('lynxtron');

function resolveRegisterGlobalEnvModule() {
  if (typeof lynxtron.registerGlobalEnvModule === 'function') {
    return lynxtron.registerGlobalEnvModule;
  }

  try {
    return process._linkedBinding('lynx_extension').registerGlobalEnvModule;
  } catch {
    return undefined;
  }
}

export const app = lynxtron.app;
export const LynxWindow = lynxtron.LynxWindow;
export const Menu = lynxtron.Menu;
export const MenuItem = lynxtron.MenuItem;
export const clipboard = lynxtron.clipboard;
export const shell = lynxtron.shell;
export const dialog = lynxtron.dialog;
export const devtool = lynxtron.devtool;
export const screen = lynxtron.screen;
export const nativeImage = lynxtron.nativeImage;
export const Notification = lynxtron.Notification;
export const protocol = lynxtron.protocol;
export const registerGlobalEnvModule = resolveRegisterGlobalEnvModule();
export const Tray = lynxtron.Tray;
export const LynxTemplateData = lynxtron.LynxTemplateData;
export const LynxUpdateMeta = lynxtron.LynxUpdateMeta;
export const powerMonitor = lynxtron.powerMonitor;
export const lynxBridge = lynxtron.lynxBridge;

export const lynx = Object.freeze({});
