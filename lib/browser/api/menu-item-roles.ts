import { app } from 'lynxtron';
import type { MenuItemConstructorOptions } from 'lynxtron';

const isMac = process.platform === 'darwin';
const isWindows = process.platform === 'win32';
const isLinux = process.platform === 'linux';

type RoleId =
  | 'about'
  | 'close'
  | 'copy'
  | 'cut'
  | 'delete'
  | 'front'
  | 'help'
  | 'hide'
  | 'hideothers'
  | 'minimize'
  | 'paste'
  | 'pasteandmatchstyle'
  | 'quit'
  | 'redo'
  | 'selectall'
  | 'services'
  | 'recentdocuments'
  | 'clearrecentdocuments'
  | 'showsubstitutions'
  | 'togglesmartquotes'
  | 'togglesmartdashes'
  | 'toggletextreplacement'
  | 'startspeaking'
  | 'stopspeaking'
  | 'togglefullscreen'
  | 'undo'
  | 'unhide'
  | 'window'
  | 'zoom'
  | 'appmenu'
  | 'filemenu'
  | 'editmenu'
  | 'viewmenu'
  | 'windowmenu'
  | 'sharemenu';

interface Role {
  label: string;
  accelerator?: string;
  checked?: boolean;
  windowMethod?: (window: any) => void;
  appMethod?: () => void;
  registerAccelerator?: boolean;
  nonNativeMacOSRole?: boolean;
  submenu?: any[];
}

export const roleList: Record<RoleId, Role> = {
  about: {
    get label() {
      return isLinux ? 'About' : `About ${app.name}`;
    },
    ...((isWindows || isLinux) && { appMethod: () => app.showAboutPanel() }),
  },
  close: {
    label: isMac ? 'Close Window' : 'Close',
    accelerator: 'CommandOrControl+W',
    windowMethod: (w: any) => w.close(),
  },
  copy: {
    label: 'Copy',
    accelerator: 'CommandOrControl+C',
    registerAccelerator: false,
  },
  cut: {
    label: 'Cut',
    accelerator: 'CommandOrControl+X',
    registerAccelerator: false,
  },
  delete: {
    label: 'Delete',
  },
  front: {
    label: 'Bring All to Front',
  },
  help: {
    label: 'Help',
  },
  hide: {
    get label() {
      return `Hide ${app.name}`;
    },
    accelerator: 'Command+H',
  },
  hideothers: {
    label: 'Hide Others',
    accelerator: 'Command+Alt+H',
  },
  minimize: {
    label: 'Minimize',
    accelerator: 'CommandOrControl+M',
    windowMethod: (w: any) => w.minimize(),
  },
  paste: {
    label: 'Paste',
    accelerator: 'CommandOrControl+V',
    registerAccelerator: false,
  },
  pasteandmatchstyle: {
    label: 'Paste and Match Style',
    accelerator: isMac ? 'Cmd+Option+Shift+V' : 'Shift+CommandOrControl+V',
    registerAccelerator: false,
  },
  quit: {
    get label() {
      switch (process.platform) {
        case 'darwin':
          return `Quit ${app.name}`;
        case 'win32':
          return 'Exit';
        default:
          return 'Quit';
      }
    },
    accelerator: isWindows ? undefined : 'CommandOrControl+Q',
    appMethod: () => app.quit(),
  },
  redo: {
    label: 'Redo',
    accelerator: isWindows ? 'Control+Y' : 'Shift+CommandOrControl+Z',
  },
  selectall: {
    label: 'Select All',
    accelerator: 'CommandOrControl+A',
  },
  services: {
    label: 'Services',
  },
  recentdocuments: {
    label: 'Open Recent',
  },
  clearrecentdocuments: {
    label: 'Clear Menu',
  },
  showsubstitutions: {
    label: 'Show Substitutions',
  },
  togglesmartquotes: {
    label: 'Smart Quotes',
  },
  togglesmartdashes: {
    label: 'Smart Dashes',
  },
  toggletextreplacement: {
    label: 'Text Replacement',
  },
  startspeaking: {
    label: 'Start Speaking',
  },
  stopspeaking: {
    label: 'Stop Speaking',
  },
  togglefullscreen: {
    label: 'Toggle Full Screen',
    accelerator: isMac ? 'Control+Command+F' : 'F11',
    windowMethod: (window: any) => {
      window.setFullScreen(!window.isFullScreen());
    },
  },
  undo: {
    label: 'Undo',
    accelerator: 'CommandOrControl+Z',
  },
  unhide: {
    label: 'Show All',
  },
  window: {
    label: 'Window',
  },
  zoom: {
    label: 'Zoom',
    windowMethod: (window: any) => {
      if (window.isMaximized()) {
        window.unmaximize();
      } else {
        window.maximize();
      }
    },
  },
  appmenu: {
    get label() {
      return app.name;
    },
    submenu: [
      { role: 'about' },
      { type: 'separator' },
      { role: 'services' },
      { type: 'separator' },
      { role: 'hide' },
      { role: 'hideOthers' },
      { role: 'unhide' },
      { type: 'separator' },
      { role: 'quit' },
    ],
  },
  filemenu: {
    label: 'File',
    submenu: [isMac ? { role: 'close' } : { role: 'quit' }],
  },
  editmenu: {
    label: 'Edit',
    submenu: [
      { role: 'undo' },
      { role: 'redo' },
      { type: 'separator' },
      { role: 'cut' },
      { role: 'copy' },
      { role: 'paste' },
      ...(isMac
        ? ([
            { role: 'pasteAndMatchStyle' },
            { role: 'delete' },
            { role: 'selectAll' },
            { type: 'separator' },
            {
              label: 'Substitutions',
              submenu: [
                { role: 'showSubstitutions' },
                { type: 'separator' },
                { role: 'toggleSmartQuotes' },
                { role: 'toggleSmartDashes' },
                { role: 'toggleTextReplacement' },
              ],
            },
            {
              label: 'Speech',
              submenu: [{ role: 'startSpeaking' }, { role: 'stopSpeaking' }],
            },
          ] as MenuItemConstructorOptions[])
        : ([
            { role: 'delete' },
            { type: 'separator' },
            { role: 'selectAll' },
          ] as MenuItemConstructorOptions[])),
    ],
  },
  viewmenu: {
    label: 'View',
    submenu: [{ role: 'togglefullscreen' }],
  },
  windowmenu: {
    label: 'Window',
    submenu: [
      { role: 'minimize' },
      { role: 'zoom' },
      ...(isMac
        ? ([
            { type: 'separator' },
            { role: 'front' },
          ] as MenuItemConstructorOptions[])
        : ([{ role: 'close' }] as MenuItemConstructorOptions[])),
    ],
  },
  sharemenu: {
    label: 'Share',
    submenu: [],
  },
};

const hasRole = (role: keyof typeof roleList) => {
  return Object.hasOwn(roleList, role);
};

const canExecuteRole = (role: keyof typeof roleList) => {
  if (!hasRole(role)) return false;
  if (!isMac) return true;
  return roleList[role].nonNativeMacOSRole;
};

export function getDefaultType(role: RoleId) {
  if (shouldOverrideCheckStatus(role)) return 'checkbox';
  return 'normal';
}

export function getDefaultLabel(role: RoleId) {
  return hasRole(role) ? roleList[role].label : '';
}

export function getCheckStatus(role: RoleId) {
  if (hasRole(role)) return roleList[role].checked;
}

export function shouldOverrideCheckStatus(role: RoleId) {
  return hasRole(role) && Object.hasOwn(roleList[role], 'checked');
}

export function getDefaultAccelerator(role: RoleId) {
  if (hasRole(role)) return roleList[role].accelerator;
  return undefined;
}

export function shouldRegisterAccelerator(role: RoleId) {
  const hasRoleRegister =
    hasRole(role) && roleList[role].registerAccelerator !== undefined;
  return hasRoleRegister ? roleList[role].registerAccelerator : true;
}

export function getDefaultSubmenu(role: RoleId) {
  if (!hasRole(role)) return;

  let { submenu } = roleList[role];

  if (Array.isArray(submenu)) {
    submenu = submenu.filter((item) => item != null);
  }

  return submenu;
}

export function execute(role: RoleId, focusedWindow: any) {
  if (!canExecuteRole(role)) return false;

  const { appMethod, windowMethod } = roleList[role];

  if (appMethod) {
    appMethod();
    return true;
  }

  if (windowMethod && focusedWindow != null) {
    windowMethod(focusedWindow);
    return true;
  }

  return false;
}
