/// <reference types="webpack/module" />

declare const BUILDFLAG: (flag: boolean) => boolean;

declare namespace NodeJS {
  interface ModuleInternal extends NodeJS.Module {
    new (id: string, parent?: NodeJS.Module | null): NodeJS.Module;
    _load(
      request: string,
      parent?: NodeJS.Module | null,
      isMain?: boolean
    ): any;
    _resolveFilename(
      request: string,
      parent?: NodeJS.Module | null,
      isMain?: boolean,
      options?: { paths: string[] }
    ): string;
    _preloadModules(requests: string[]): void;
    _nodeModulePaths(from: string): string[];
    _extensions: Record<
      string,
      (module: NodeJS.Module, filename: string) => any
    >;
    _cache: Record<string, NodeJS.Module>;
    wrapper: [string, string];
  }

  interface FeaturesBinding {
    isBuiltinSpellCheckerEnabled(): boolean;
    isPDFViewerEnabled(): boolean;
    isFakeLocationProviderEnabled(): boolean;
    isPrintingEnabled(): boolean;
    isExtensionsEnabled(): boolean;
    isComponentBuild(): boolean;
  }

  interface V8UtilBinding {
    getHiddenValue<T>(obj: any, key: string): T;
    setHiddenValue<T>(obj: any, key: string, value: T): void;
    requestGarbageCollectionForTesting(): void;
    runUntilIdle(): void;
    triggerFatalErrorForTesting(): void;
  }

  interface EnvironmentBinding {
    getVar(name: string): string | null;
    hasVar(name: string): boolean;
    setVar(name: string, value: string): boolean;
  }

  interface AsarFileInfo {
    size: number;
    unpacked: boolean;
    offset: number;
    integrity?: {
      algorithm: 'SHA256';
      hash: string;
    };
  }

  interface AsarFileStat {
    size: number;
    offset: number;
    type: number;
  }

  interface AsarArchive {
    getFileInfo(path: string): AsarFileInfo | false;
    stat(path: string): AsarFileStat | false;
    readdir(path: string): string[] | false;
    realpath(path: string): string | false;
    copyFileOut(path: string): string | false;
    getFdAndValidateIntegrityLater(): number | -1;
  }

  interface AsarBinding {
    Archive: { new (path: string): AsarArchive };
    splitPath(
      path: string
    ):
      | {
          isAsar: false;
        }
      | {
          isAsar: true;
          asarPath: string;
          filePath: string;
        };
  }

  interface Process {
    internalBinding?(name: string): any;
    _linkedBinding(name: string): any;
    // _linkedBinding(name: 'electron_common_asar'): AsarBinding;
    // _linkedBinding(name: 'electron_common_clipboard'): Electron.Clipboard;
    // _linkedBinding(name: 'electron_common_command_line'): Electron.CommandLine;
    // _linkedBinding(name: 'electron_common_environment'): EnvironmentBinding;
    // _linkedBinding(name: 'electron_common_features'): FeaturesBinding;
    // _linkedBinding(name: 'electron_common_native_image'): { nativeImage: typeof Electron.NativeImage };
    // _linkedBinding(name: 'electron_common_net'): NetBinding;
    // _linkedBinding(name: 'electron_common_shell'): Electron.Shell;
    // _linkedBinding(name: 'electron_common_v8_util'): V8UtilBinding;
    _linkedBinding(
      name: 'electron_browser_app'
    ): { app: Lynxtron.App; App: Function };
    // _linkedBinding(name: 'electron_browser_auto_updater'): { autoUpdater: Electron.AutoUpdater };
    // _linkedBinding(name: 'electron_browser_crash_reporter'): CrashReporterBinding;
    // _linkedBinding(name: 'electron_browser_desktop_capturer'): { createDesktopCapturer(): ElectronInternal.DesktopCapturer; isDisplayMediaSystemPickerAvailable(): boolean; };
    // _linkedBinding(name: 'electron_browser_event_emitter'): { setEventEmitterPrototype(prototype: Object): void; };
    // _linkedBinding(name: 'electron_browser_global_shortcut'): { globalShortcut: Electron.GlobalShortcut };
    // _linkedBinding(name: 'electron_browser_image_view'): { ImageView: any };
    // _linkedBinding(name: 'electron_browser_in_app_purchase'): { inAppPurchase: Electron.InAppPurchase };
    // _linkedBinding(name: 'electron_browser_message_port'): { createPair(): { port1: Electron.MessagePortMain, port2: Electron.MessagePortMain }; };
    // _linkedBinding(name: 'electron_browser_native_theme'): { nativeTheme: Electron.NativeTheme };
    // _linkedBinding(name: 'electron_browser_notification'): NotificationBinding;
    // _linkedBinding(name: 'electron_browser_power_monitor'): PowerMonitorBinding;
    // _linkedBinding(name: 'electron_browser_power_save_blocker'): { powerSaveBlocker: Electron.PowerSaveBlocker };
    // _linkedBinding(name: 'electron_browser_push_notifications'): { pushNotifications: Electron.PushNotifications };
    // _linkedBinding(name: 'electron_browser_safe_storage'): { safeStorage: Electron.SafeStorage };
    // _linkedBinding(name: 'electron_browser_session'): SessionBinding;
    // _linkedBinding(name: 'electron_browser_screen'): { createScreen(): Electron.Screen };
    // _linkedBinding(name: 'electron_browser_service_worker_main'): ServiceWorkerMainBinding;
    // _linkedBinding(name: 'electron_browser_system_preferences'): { systemPreferences: Electron.SystemPreferences };
    // _linkedBinding(name: 'electron_browser_tray'): { Tray: Electron.Tray };
    // _linkedBinding(name: 'electron_browser_view'): { View: Electron.View };
    // _linkedBinding(name: 'electron_browser_web_contents_view'): { WebContentsView: typeof Electron.WebContentsView };
    // _linkedBinding(name: 'electron_browser_web_view_manager'): WebViewManagerBinding;
    // _linkedBinding(name: 'electron_browser_web_frame_main'): WebFrameMainBinding;
    // _linkedBinding(name: 'electron_renderer_crash_reporter'): Electron.CrashReporter;
    // _linkedBinding(name: 'electron_renderer_ipc'): IpcRendererBinding;
    // _linkedBinding(name: 'electron_renderer_web_frame'): WebFrameBinding;
    log: NodeJS.WriteStream['write'];
    activateUvLoop(): void;

    // Additional events
    once(event: 'document-start', listener: () => any): this;
    once(event: 'document-end', listener: () => any): this;

    // Additional properties
    _serviceStartupScript: string;
    _getOrCreateArchive?: (path: string) => NodeJS.AsarArchive | null;

    helperExecPath: string;
    mainModule?: NodeJS.Module | undefined;

    appCodeLoaded?: () => void;
    noAsar: boolean;
    readonly resourcesPath: string;
  }
}

declare module NodeJS {
  interface Global {
    require: NodeRequire;
    module: NodeModule;
    __filename: string;
    __dirname: string;
  }
}

interface ContextMenuItem {
  id: number;
  label: string;
  type: 'normal' | 'separator' | 'subMenu' | 'checkbox' | 'header' | 'palette';
  checked: boolean;
  enabled: boolean;
  subItems: ContextMenuItem[];
}

declare module 'original-fs' {
  export * from 'node:fs';
}
