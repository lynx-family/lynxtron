// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

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
