// TODO: Updating this file also required updating the module-keys file

// Browser side modules, please sort alphabetically.
export const browserModuleList: ElectronInternal.ModuleEntry[] = [
  { name: 'app', loader: () => require('./app') },
  { name: 'BaseWindow', loader: () => require('./base-window') },
  { name: 'LynxWindow', loader: () => require('./lynx-window') },
];
