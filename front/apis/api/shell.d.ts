export interface OpenExternalOptions {
  // Docs: https://electronjs.org/docs/api/shell

  /**
   * `true` to bring the opened application to the foreground. The default is `true`.
   *
   * @platform darwin
   */
  activate?: boolean;
  /**
   * The working directory.
   *
   * @platform win32
   */
  workingDirectory?: string;
  /**
   * Indicates a user initiated launch that enables tracking of frequently used
   * programs and other behaviors. The default is `false`.
   *
   * @platform win32
   */
  logUsage?: boolean;
}

export interface ShortcutDetails {
  // Docs: https://electronjs.org/docs/api/structures/shortcut-details

  /**
   * The Application User Model ID. Default is empty.
   */
  appUserModelId?: string;
  /**
   * The arguments to be applied to `target` when launching from this shortcut.
   * Default is empty.
   */
  args?: string;
  /**
   * The working directory. Default is empty.
   */
  cwd?: string;
  /**
   * The description of the shortcut. Default is empty.
   */
  description?: string;
  /**
   * The path to the icon, can be a DLL or EXE. `icon` and `iconIndex` have to be set
   * together. Default is empty, which uses the target's icon.
   */
  icon?: string;
  /**
   * The resource ID of icon when `icon` is a DLL or EXE. Default is 0.
   */
  iconIndex?: number;
  /**
   * The target to launch from this shortcut.
   */
  target: string;
  /**
   * The Application Toast Activator CLSID. Needed for participating in Action
   * Center.
   */
  toastActivatorClsid?: string;
}

export interface Shell {
  // Docs: https://electronjs.org/docs/api/shell

  /**
   * Play the beep sound.
   */
  beep(): void;
  /**
   * Open the given external protocol URL in the desktop's default manner. (For
   * example, mailto: URLs in the user's default mail agent).
   */
  openExternal(url: string, options?: OpenExternalOptions): Promise<void>;
  /**
   * Resolves with a string containing the error message corresponding to the failure
   * if a failure occurred, otherwise "".
   *
   * Open the given file in the desktop's default manner.
   */
  openPath(path: string): Promise<string>;
  /**
   * Resolves the shortcut link at `shortcutPath`.
   *
   * An exception will be thrown when any error happens.
   *
   * @platform win32
   */
  readShortcutLink(shortcutPath: string): ShortcutDetails;
  /**
   * Show the given file in a file manager. If possible, select the file.
   */
  showItemInFolder(fullPath: string): void;
  /**
   * Resolves when the operation has been completed. Rejects if there was an error
   * while deleting the requested item.
   *
   * This moves a path to the OS-specific trash location (Trash on macOS, Recycle Bin
   * on Windows, and a desktop-environment-specific location on Linux).
   */
  trashItem(path: string): Promise<void>;
  /**
   * Whether the shortcut was created successfully.
   *
   * Creates or updates a shortcut link at `shortcutPath`.
   *
   * @platform win32
   */
  writeShortcutLink(
    shortcutPath: string,
    operation: 'create' | 'update' | 'replace',
    options: ShortcutDetails
  ): boolean;
  /**
   * Whether the shortcut was created successfully.
   *
   * Creates or updates a shortcut link at `shortcutPath`.
   *
   * @platform win32
   */
  writeShortcutLink(shortcutPath: string, options: ShortcutDetails): boolean;
}

export declare const shell: Shell;
