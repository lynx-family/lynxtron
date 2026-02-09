import { sortMenuItems } from '@lynxtron/internal/browser/api/menu-utils';

import { BaseWindow, MenuItem } from 'lynxtron';

const bindings = process._linkedBinding('lynxtron_binding_menu');

const { Menu } = bindings as { Menu: any };
const checked = new WeakMap<MenuItem, boolean>();
let applicationMenu: any = null;
let groupIdIndex = 0;

(Menu.prototype as any)._init = function () {
  this.commandsMap = {};
  this.groupsMap = {};
  this.items = [];
};

(Menu.prototype as any)._isCommandIdChecked = function (id: number) {
  const item = this.commandsMap[id];
  if (!item) return false;
  return item.getCheckStatus();
};

(Menu.prototype as any)._isCommandIdEnabled = function (id: number) {
  const item = this.commandsMap[id];
  if (!item) return false;

  const focusedWindow = BaseWindow.getFocusedWindow();

  if (item.role === 'minimize' && focusedWindow) {
    return focusedWindow.isMinimizable();
  }

  if (item.role === 'togglefullscreen' && focusedWindow) {
    return focusedWindow.isFullScreenable();
  }

  if (item.role === 'close' && focusedWindow) {
    return focusedWindow.isClosable();
  }

  return item.enabled;
};

(Menu.prototype as any)._shouldCommandIdWorkWhenHidden = function (id: number) {
  return this.commandsMap[id]?.acceleratorWorksWhenHidden ?? false;
};

(Menu.prototype as any)._isCommandIdVisible = function (id: number) {
  return this.commandsMap[id]?.visible ?? false;
};

(Menu.prototype as any)._getAcceleratorForCommandId = function (
  id: number,
  useDefaultAccelerator: boolean
) {
  const command = this.commandsMap[id];
  if (!command) return;
  if (command.accelerator != null) return command.accelerator;
  if (useDefaultAccelerator) return command.getDefaultRoleAccelerator();
};

(Menu.prototype as any)._shouldRegisterAcceleratorForCommandId = function (
  id: number
) {
  return this.commandsMap[id]?.registerAccelerator ?? false;
};

if (process.platform === 'darwin') {
  (Menu.prototype as any)._getSharingItemForCommandId = function (id: number) {
    return this.commandsMap[id]?.sharingItem ?? null;
  };
}

(Menu.prototype as any)._executeCommand = function (event: any, id: number) {
  const command = this.commandsMap[id];
  if (!command) return;
  const focusedWindow = BaseWindow.getFocusedWindow();
  command.click(event, focusedWindow);
};

(Menu.prototype as any)._menuWillShow = function () {
  for (const id of Object.keys(this.groupsMap)) {
    const group = this.groupsMap[Number(id)];
    if (!group || group.length === 0) continue;
    const found = group.find((item: MenuItem) => item.checked) || null;
    if (!found) checked.set(group[0], true);
  }
};

Menu.prototype.popup = function (options: any = {}) {
  if (options == null || typeof options !== 'object') {
    throw new TypeError('Options must be an object');
  }
  let { window, x, y, positioningItem, callback } = options;

  if (!callback || typeof callback !== 'function') callback = () => {};

  if (typeof x !== 'number') x = -1;
  if (typeof y !== 'number') y = -1;
  if (typeof positioningItem !== 'number') positioningItem = -1;

  const wins = BaseWindow.getAllWindows();
  if (!wins || !wins.includes(window as any)) {
    window = BaseWindow.getFocusedWindow() as any;
    if (!window && wins && wins.length > 0) {
      window = wins[0] as any;
    }
    if (!window) {
      throw new Error('Cannot open Menu without a BaseWindow present');
    }
  }

  this.popupAt(window as any, x, y, positioningItem, callback);
  return { browserWindow: window, x, y, position: positioningItem };
};

Menu.prototype.closePopup = function (window?: any) {
  if (window instanceof BaseWindow) {
    this.closePopupAt(window.id);
  } else {
    this.closePopupAt(-1);
  }
};

Menu.prototype.getMenuItemById = function (id: string) {
  const items = this.items;

  let found = items.find((item: any) => item.id === id) || null;
  for (let i = 0; !found && i < items.length; i++) {
    const { submenu } = items[i];
    if (submenu) {
      found = submenu.getMenuItemById(id);
    }
  }
  return found;
};

Menu.prototype.append = function (item: any) {
  return this.insert(this.getItemCount(), item);
};

Menu.prototype.insert = function (pos: number, item: any) {
  if ((item ? item.constructor : undefined) !== MenuItem) {
    throw new TypeError('Invalid item');
  }

  if (pos < 0) {
    throw new RangeError(`Position ${pos} cannot be less than 0`);
  } else if (pos > this.getItemCount()) {
    throw new RangeError(
      `Position ${pos} cannot be greater than the total MenuItem count`
    );
  }

  insertItemByType.call(this, item, pos);

  if (item.sublabel) this.setSublabel(pos, item.sublabel);
  if (item.toolTip) this.setToolTip(pos, item.toolTip);
  if (item.icon) this.setIcon(pos, item.icon);
  if (item.role) this.setRole(pos, item.role);
  const itemType = item.type as string;
  if (itemType === 'palette' || itemType === 'header') {
    this.setCustomType(pos, item.type);
  }

  item.overrideReadOnlyProperty('menu', this);

  this.items.splice(pos, 0, item);
  this.commandsMap[item.commandId] = item;
};

(Menu.prototype as any)._callMenuWillShow = function () {
  if (this.delegate) this.delegate.menuWillShow(this);
  for (const item of this.items) {
    if (item.submenu) item.submenu._callMenuWillShow();
  }
};

Menu.getApplicationMenu = () => applicationMenu;

Menu.sendActionToFirstResponder = bindings.sendActionToFirstResponder;

Menu.setApplicationMenu = function (menu: any) {
  if (menu && menu.constructor !== Menu) {
    throw new TypeError('Invalid menu');
  }

  applicationMenu = menu;

  if (process.platform === 'darwin') {
    if (!menu) return;
    menu._callMenuWillShow();
    bindings.setApplicationMenu(menu);
  }
};

Menu.buildFromTemplate = function (template: any[]) {
  if (!Array.isArray(template)) {
    throw new TypeError(
      'Invalid template for Menu: Menu template must be an array'
    );
  }

  if (!areValidTemplateItems(template)) {
    throw new TypeError(
      'Invalid template for MenuItem: must have at least one of label, role or type'
    );
  }

  const sorted = sortTemplate(template);
  const filtered = removeExtraSeparators(sorted);

  const menu = new Menu();
  for (const item of filtered) {
    if (item instanceof MenuItem) {
      menu.append(item);
    } else {
      menu.append(new MenuItem(item));
    }
  }

  return menu;
};

function areValidTemplateItems(template: any[]) {
  return template.every(
    (item) =>
      item != null &&
      typeof item === 'object' &&
      (Object.hasOwn(item, 'label') ||
        Object.hasOwn(item, 'role') ||
        item.type === 'separator')
  );
}

function sortTemplate(template: any[]) {
  const sorted = sortMenuItems(template);
  for (const item of sorted) {
    if (Array.isArray(item.submenu)) {
      item.submenu = sortTemplate(item.submenu);
    }
  }
  return sorted;
}

function generateGroupId(items: any[], pos: number) {
  if (pos > 0) {
    for (let idx = pos - 1; idx >= 0; idx--) {
      if (items[idx].type === 'radio') return (items[idx] as MenuItem).groupId;
      if (items[idx].type === 'separator') break;
    }
  } else if (pos < items.length) {
    for (let idx = pos; idx <= items.length - 1; idx++) {
      if (items[idx].type === 'radio') return (items[idx] as MenuItem).groupId;
      if (items[idx].type === 'separator') break;
    }
  }
  groupIdIndex += 1;
  return groupIdIndex;
}

function removeExtraSeparators(items: any[]) {
  let ret = items.filter((e: any, idx: number, arr: any[]) => {
    if (e.visible === false) return true;
    return (
      e.type !== 'separator' || idx === 0 || arr[idx - 1].type !== 'separator'
    );
  });

  ret = ret.filter((e: any, idx: number, arr: any[]) => {
    if (e.visible === false) return true;
    return e.type !== 'separator' || (idx !== 0 && idx !== arr.length - 1);
  });

  return ret;
}

function insertItemByType(this: any, item: any, pos: number) {
  const types = {
    normal: () => this.insertItem(pos, item.commandId, item.label),
    header: () => this.insertItem(pos, item.commandId, item.label),
    checkbox: () => this.insertCheckItem(pos, item.commandId, item.label),
    separator: () => this.insertSeparator(pos),
    submenu: () =>
      this.insertSubMenu(pos, item.commandId, item.label, item.submenu ?? null),
    palette: () =>
      this.insertSubMenu(pos, item.commandId, item.label, item.submenu ?? null),
    radio: () => {
      item.overrideReadOnlyProperty(
        'groupId',
        generateGroupId(this.items, pos)
      );
      if (this.groupsMap[item.groupId] == null) {
        this.groupsMap[item.groupId] = [];
      }
      this.groupsMap[item.groupId].push(item);

      checked.set(item, item.checked);
      Object.defineProperty(item, 'checked', {
        enumerable: true,
        get: () => checked.get(item),
        set: () => {
          for (const other of this.groupsMap[item.groupId]) {
            if (other !== item) checked.set(other, false);
          }
          checked.set(item, true);
        },
      });
      this.insertRadioItem(pos, item.commandId, item.label, item.groupId);
    },
  };
  (types as Record<string, () => void>)[item.type]();
}

module.exports = Menu;
