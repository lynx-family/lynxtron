// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// TODO: Updating this file also required updating the module-keys file

// Browser side modules, please sort alphabetically.
export const lynxtronModuleList: LynxtronInternal.ModuleEntry[] = [
  { name: 'app', loader: () => require('./app') },
  { name: 'BaseWindow', loader: () => require('./base-window') },
  { name: 'clipboard', loader: () => require('./clipboard') },
  { name: 'dialog', loader: () => require('./dialog') },
  { name: 'devtool', loader: () => require('./devtool') },
  { name: 'LynxWindow', loader: () => require('./lynx-window') },
  {
    name: 'LynxTemplateBundle',
    loader: () => require('./lynx-template-bundle'),
  },
  { name: 'LynxTemplateData', loader: () => require('./lynx-template-data') },
  { name: 'LynxUpdateMeta', loader: () => require('./lynx-update-meta') },
  { name: 'Menu', loader: () => require('./menu') },
  { name: 'MenuItem', loader: () => require('./menu-item') },
  { name: 'nativeImage', loader: () => require('./native-image') },
  { name: 'Notification', loader: () => require('./notification') },
  { name: 'protocol', loader: () => require('./protocol') },
  { name: 'shell', loader: () => require('./shell') },
  { name: 'screen', loader: () => require('./screen') },
  { name: 'Tray', loader: () => require('./tray') },
  { name: 'utilityProcess', loader: () => require('./utility-process') },
  { name: 'powerMonitor', loader: () => require('./power-monitor') },
];
