// export * from './packages/lynxtron/apis/lynxtron';

import { app as Lynxtron_app } from '../packages/lynxtron/apis/api/app';
import { App as LynxtronApp } from '../packages/lynxtron/apis/api/app';
import { BaseWindow as LynxtronBaseWindow } from '../packages/lynxtron/apis/api/base-window';
import { LynxWindow as LynxtronLynxWindow } from '../packages/lynxtron/apis/api/lynx-window';
import { Menu as LynxtronMenu } from '../packages/lynxtron/apis/api/menu';
import { Event as LynxtronEvent } from '../packages/lynxtron/apis/lynxtron';
import { CommandLine as LynxtronCommandLine } from '../packages/lynxtron/apis/api/command-line';
import { Screen as LynxtronScreen } from '../packages/lynxtron/apis/api/screen';
import {
  TouchBar as LynxtronTouchBar,
  TouchBarConstructorOptions as LynxtronTouchBarConstructorOptions,
} from '../packages/lynxtron/apis/api/touch-bar';

import {
  OpenDialogOptions as LynxtronOpenDialogOptions,
  OpenDialogReturnValue as LynxtronOpenDialogReturnValue,
  MessageBoxOptions as LynxtronMessageBoxOptions,
  SaveDialogOptions as LynxtronSaveDialogOptions,
  SaveDialogReturnValue as LynxtronSaveDialogReturnValue,
  MessageBoxReturnValue as LynxtronMessageBoxReturnValue,
  CertificateTrustDialogOptions as LynxtronCertificateTrustDialogOptions,
} from '../packages/lynxtron/apis/api/dialog';

declare global {
  namespace Lynxtron {
    type Menu = LynxtronMenu;
    type CommandLine = LynxtronCommandLine;
    type Screen = LynxtronScreen;
    type TouchBar = LynxtronTouchBar;
    type TouchBarConstructorOptions = LynxtronTouchBarConstructorOptions;

    interface App extends LynxtronApp {}
  }

  namespace ElectronInternal {
    type ModuleLoader = () => any;

    interface ModuleEntry {
      name: string;
      loader: ModuleLoader;
    }
  }

  interface Global extends NodeJS.Global {
    require: NodeRequire;
    module: NodeModule;
    __filename: string;
    __dirname: string;
  }
}

type TouchBarItemType = NonNullable<
  Lynxtron.TouchBarConstructorOptions['items']
>[0];

declare class BaseWindowInternal extends LynxtronBaseWindow {
  _init(): void;
  _touchBar: Lynxtron.TouchBar | null;
  _setTouchBarItems: (items: TouchBarItemType[]) => void;
  _setEscapeTouchBarItem: (item: TouchBarItemType | {}) => void;
  _refreshTouchBarItem: (itemID: string) => void;
}

declare class LynxWindowInternal extends LynxtronLynxWindow {
  _init(): void;
  _getWindowButtonVisibility: () => boolean;
  _getAlwaysOnTopLevel: () => string;
  frameName: string;
  on(
    event: '-touch-bar-interaction',
    listener: (event: Event, itemID: string, details: any) => void
  ): this;
  removeListener(
    event: '-touch-bar-interaction',
    listener: (event: Event, itemID: string, details: any) => void
  ): this;
  on(event: string | symbol, listener: (...args: any[]) => void): this;
  removeListener(
    event: string | symbol,
    listener: (...args: any[]) => void
  ): this;
}

declare module 'lynxtron' {
  export const app: typeof Lynxtron_app;
  export const BaseWindow: typeof BaseWindowInternal;
  export const LynxWindow: typeof LynxWindowInternal;
  export const Menu: typeof LynxtronMenu;

  export type BaseWindow = BaseWindowInternal;
  export type LynxWindow = LynxWindowInternal;
  export type OpenDialogOptions = LynxtronOpenDialogOptions;
  export type OpenDialogReturnValue = LynxtronOpenDialogReturnValue;
  export type MessageBoxOptions = LynxtronMessageBoxOptions;
  export type MessageBoxReturnValue = LynxtronMessageBoxReturnValue;
  export type SaveDialogOptions = LynxtronSaveDialogOptions;
  export type SaveDialogReturnValue = LynxtronSaveDialogReturnValue;
  export type CertificateTrustDialogOptions = LynxtronCertificateTrustDialogOptions;
  export type Event = LynxtronEvent;
}
