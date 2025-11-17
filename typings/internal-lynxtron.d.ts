/// <reference path="../lynxtron.d.ts" />

/**
 * This file augments the Electron TS namespace with the internal APIs
 * that are not documented but are used by Electron internally
 */

declare namespace Lynxtron {
  // enum ProcessType {
  //   browser = 'browser',
  //   renderer = 'renderer',
  //   worker = 'worker',
  //   utility = 'utility'
  // }

  interface App {
    setVersion(version: string): void;
    setDesktopName(name: string): void;
    setAppPath(path: string | null): void;
  }

  type TouchBarItemType = NonNullable<
    Lynxtron.TouchBarConstructorOptions['items']
  >[0];

  interface BaseWindow {
    _init(): void;
    _touchBar: Lynxtron.TouchBar | null;
    _setTouchBarItems: (items: TouchBarItemType[]) => void;
    _setEscapeTouchBarItem: (item: TouchBarItemType | {}) => void;
    _refreshTouchBarItem: (itemID: string) => void;
    on(
      event: '-touch-bar-interaction',
      listener: (event: Event, itemID: string, details: any) => void
    ): this;
    removeListener(
      event: '-touch-bar-interaction',
      listener: (event: Event, itemID: string, details: any) => void
    ): this;
  }

  interface LynxWindow extends BaseWindow {
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
  }
}

declare namespace ElectronInternal {
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
