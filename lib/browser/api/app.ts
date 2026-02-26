import { Menu } from 'lynxtron';

import * as fs from 'fs';

const bindings = process._linkedBinding('lynxtron_binding_app');
const commandLine = process._linkedBinding('lynxtron_binding_command_line');
const { app } = bindings;

// Only one app object permitted.
export default app;

let dockMenu: Lynxtron.Menu | null = null;

// Properties.
const nativeBCGetter = app.getBadgeCount;
const nativeBCSetter = app.setBadgeCount;
Object.defineProperty(app, 'badgeCount', {
  get: () => nativeBCGetter.call(app),
  set: (count) => nativeBCSetter.call(app, count),
});

const nativeNGetter = app.getName;
const nativeNSetter = app.setName;
Object.defineProperty(app, 'name', {
  get: () => nativeNGetter.call(app),
  set: (name) => nativeNSetter.call(app, name),
});

Object.assign(app, {
  commandLine: {
    hasSwitch: (theSwitch: string) => commandLine.hasSwitch(String(theSwitch)),
    getSwitchValue: (theSwitch: string) =>
      commandLine.getSwitchValue(String(theSwitch)),
    appendSwitch: (theSwitch: string, value?: string) =>
      commandLine.appendSwitch(
        String(theSwitch),
        typeof value === 'undefined' ? value : String(value)
      ),
    appendArgument: (arg: string) => commandLine.appendArgument(String(arg)),
    removeSwitch: (theSwitch: string) =>
      commandLine.removeSwitch(String(theSwitch)),
  } as Lynxtron.CommandLine,
});

// we define this here because it'd be overly complicated to
// do in native land
Object.defineProperty(app, 'applicationMenu', {
  get() {
    return Menu.getApplicationMenu();
  },
  set(menu: Lynxtron.Menu | null) {
    return Menu.setApplicationMenu(menu);
  },
});

// The native implementation is not provided on non-windows platforms
app.setAppUserModelId = app.setAppUserModelId || (() => {});

// TODO(Guo Xi): recover
if (process.platform === 'darwin') {
  const setDockMenu = app.dock!.setMenu;
  app.dock!.setMenu = (menu: Lynxtron.Menu | null) => {
    dockMenu = menu;
    if (menu) (menu as any)._callMenuWillShow();
    setDockMenu(menu);
  };
  app.dock!.getMenu = () => dockMenu;
}

if (process.platform === 'linux') {
  const patternVmRSS = /^VmRSS:\s*(\d+) kB$/m;
  const patternVmHWM = /^VmHWM:\s*(\d+) kB$/m;

  const getStatus = (pid: number) => {
    try {
      return fs.readFileSync(`/proc/${pid}/status`, 'utf8');
    } catch {
      return '';
    }
  };

  const getEntry = (file: string, pattern: RegExp) => {
    const match = file.match(pattern);
    return match ? parseInt(match[1], 10) : 0;
  };

  const getProcessMemoryInfo = (pid: number) => {
    const file = getStatus(pid);

    return {
      workingSetSize: getEntry(file, patternVmRSS),
      peakWorkingSetSize: getEntry(file, patternVmHWM),
    };
  };

  // TODO(Guo Xi): review
  const nativeFn = app.getAppMetrics;
  app.getAppMetrics = () => {
    const metrics = nativeFn.call(app);
    metrics.memory = getProcessMemoryInfo(metrics.pid);
    return metrics;
  };
}
