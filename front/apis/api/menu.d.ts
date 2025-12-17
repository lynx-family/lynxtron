import { EventEmitter } from 'node:events';

export declare class MenuItem {
  // Docs: https://electronjs.org/docs/api/menu-item
}

export declare class Menu extends EventEmitter {
  // Docs: https://electronjs.org/docs/api/menu

  /**
   * Menu
   */
  constructor();
  /**
   * The application menu, if set, or `null`, if not set.
   *
   * > [!NOTE] The returned `Menu` instance doesn't support dynamic addition or
   * removal of menu items. Instance properties can still be dynamically modified.
   */
  static getApplicationMenu(): Menu | null;
  /**
   * Sets `menu` as the application menu on macOS. On Windows and Linux, the `menu`
   * will be set as each window's top menu.
   *
   * Also on Windows and Linux, you can use a `&` in the top-level item name to
   * indicate which letter should get a generated accelerator. For example, using
   * `&File` for the file menu would result in a generated `Alt-F` accelerator that
   * opens the associated menu. The indicated character in the button label then gets
   * an underline, and the `&` character is not displayed on the button label.
   *
   * In order to escape the `&` character in an item name, add a proceeding `&`. For
   * example, `&&File` would result in `&File` displayed on the button label.
   *
   * Passing `null` will suppress the default menu. On Windows and Linux, this has
   * the additional effect of removing the menu bar from the window.
   *
   * > [!NOTE] The default menu will be created automatically if the app does not set
   * one. It contains standard items such as `File`, `Edit`, `View`, `Window` and
   * `Help`.
   */
  static setApplicationMenu(menu: Menu | null): void;
  /**
   * A `MenuItem[]` array containing the menu's items.
   *
   * Each `Menu` consists of multiple `MenuItem` instances and each `MenuItem` can
   * nest a `Menu` into its `submenu` property.
   */
  items: MenuItem[];
}
