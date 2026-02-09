import { EventEmitter } from 'node:events';
import { NativeImage } from './native-image';

export type MenuItemType =
  | 'normal'
  | 'separator'
  | 'submenu'
  | 'checkbox'
  | 'radio'
  | 'header'
  | 'palette';

export interface MenuItemConstructorOptions {
  id?: string;
  label?: string;
  sublabel?: string;
  toolTip?: string;
  enabled?: boolean;
  visible?: boolean;
  checked?: boolean;
  type?: MenuItemType;
  role?: string;
  accelerator?: string | null;
  acceleratorWorksWhenHidden?: boolean;
  registerAccelerator?: boolean;
  icon?: NativeImage | null;
  submenu?: Menu | MenuItemConstructorOptions[] | null;
  selector?: string;
  click?: (menuItem: MenuItem, focusedWindow: any, event: any) => void;
}

export interface PopupOptions {
  window?: any;
  x?: number;
  y?: number;
  positioningItem?: number;
  callback?: () => void;
}

export declare class MenuItem {
  constructor(options?: MenuItemConstructorOptions);
  id?: string;
  label: string;
  type: MenuItemType;
  commandId: number;
  checked: boolean;
  enabled: boolean;
  visible: boolean;
  submenu?: Menu | null;
  role?: string;
  accelerator?: string | null;
  acceleratorWorksWhenHidden: boolean;
  registerAccelerator: boolean;
  icon?: NativeImage | null;
  sublabel?: string;
  toolTip?: string;
  menu?: Menu;
  selector?: string;
  groupId: number;
  click: (event: any, focusedWindow: any) => void;
  getDefaultRoleAccelerator(): string | null;
  getCheckStatus(): boolean;
  overrideProperty(name: string, defaultValue?: any): void;
  overrideReadOnlyProperty(name: string, defaultValue?: any): void;
}

export declare class Menu extends EventEmitter {
  constructor();
  static getApplicationMenu(): Menu | null;
  static setApplicationMenu(menu: Menu | null): void;
  static sendActionToFirstResponder(action: string): void;
  static buildFromTemplate(
    template: Array<MenuItemConstructorOptions | MenuItem>
  ): Menu;
  popup(
    options?: PopupOptions
  ): {
    browserWindow: any;
    x: number;
    y: number;
    position: number;
  };
  closePopup(window?: any): void;
  getMenuItemById(id: string): MenuItem | null;
  append(item: MenuItem): void;
  insert(pos: number, item: MenuItem): void;
  items: MenuItem[];
}
