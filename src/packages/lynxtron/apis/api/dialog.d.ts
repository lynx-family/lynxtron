// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { NativeImage } from './native-image';
import { BaseWindow } from './base-window';

export interface FileFilter {
  // Docs: https://electronjs.org/docs/api/structures/file-filter

  extensions: string[];
  name: string;
}

export interface MessageBoxOptions {
  /**
   * Content of the message box.
   */
  message: string;
  /**
   * Can be `none`, `info`, `error`, `question` or `warning`. On Windows, `question`
   * displays the same icon as `info`, unless you set an icon using the `icon`
   * option. On macOS, both `warning` and `error` display the same warning icon.
   */
  type?: 'none' | 'info' | 'error' | 'question' | 'warning';
  /**
   * Array of texts for buttons. On Windows, an empty array will result in one button
   * labeled "OK".
   */
  buttons?: string[];
  /**
   * Index of the button in the buttons array which will be selected by default when
   * the message box opens.
   */
  defaultId?: number;
  /**
   * Pass an instance of AbortSignal to optionally close the message box, the message
   * box will behave as if it was cancelled by the user. On macOS, `signal` does not
   * work with message boxes that do not have a parent window, since those message
   * boxes run synchronously due to platform limitations.
   */
  signal?: AbortSignal;
  /**
   * Title of the message box, some platforms will not show it.
   */
  title?: string;
  /**
   * Extra information of the message.
   */
  detail?: string;
  /**
   * If provided, the message box will include a checkbox with the given label.
   */
  checkboxLabel?: string;
  /**
   * Initial checked state of the checkbox. `false` by default.
   */
  checkboxChecked?: boolean;
  icon?: NativeImage | string;
  /**
   * Custom width of the text in the message box.
   *
   * @platform darwin
   */
  textWidth?: number;
  /**
   * The index of the button to be used to cancel the dialog, via the `Esc` key. By
   * default this is assigned to the first button with "cancel" or "no" as the label.
   * If no such labeled buttons exist and this option is not set, `0` will be used as
   * the return value.
   */
  cancelId?: number;
  /**
   * On Windows Electron will try to figure out which one of the `buttons` are common
   * buttons (like "Cancel" or "Yes"), and show the others as command links in the
   * dialog. This can make the dialog appear in the style of modern Windows apps. If
   * you don't like this behavior, you can set `noLink` to `true`.
   */
  noLink?: boolean;
  /**
   * Normalize the keyboard access keys across platforms. Default is `false`.
   * Enabling this assumes `&` is used in the button labels for the placement of the
   * keyboard shortcut access key and labels will be converted so they work correctly
   * on each platform, `&` characters are removed on macOS, converted to `_` on
   * Linux, and left untouched on Windows. For example, a button label of `Vie&w`
   * will be converted to `Vie_w` on Linux and `View` on macOS and can be selected
   * via `Alt-W` on Windows and Linux.
   */
  normalizeAccessKeys?: boolean;
}

export interface MessageBoxReturnValue {
  /**
   * The index of the clicked button.
   */
  response: number;
  /**
   * The checked state of the checkbox if `checkboxLabel` was set. Otherwise `false`.
   */
  checkboxChecked: boolean;
}

export interface MessageBoxSyncOptions {
  /**
   * Content of the message box.
   */
  message: string;
  /**
   * Can be `none`, `info`, `error`, `question` or `warning`. On Windows, `question`
   * displays the same icon as `info`, unless you set an icon using the `icon`
   * option. On macOS, both `warning` and `error` display the same warning icon.
   */
  type?: 'none' | 'info' | 'error' | 'question' | 'warning';
  /**
   * Array of texts for buttons. On Windows, an empty array will result in one button
   * labeled "OK".
   */
  buttons?: string[];
  /**
   * Index of the button in the buttons array which will be selected by default when
   * the message box opens.
   */
  defaultId?: number;
  /**
   * Title of the message box, some platforms will not show it.
   */
  title?: string;
  /**
   * Extra information of the message.
   */
  detail?: string;
  icon?: NativeImage | string;
  /**
   * Custom width of the text in the message box.
   *
   * @platform darwin
   */
  textWidth?: number;
  /**
   * The index of the button to be used to cancel the dialog, via the `Esc` key. By
   * default this is assigned to the first button with "cancel" or "no" as the label.
   * If no such labeled buttons exist and this option is not set, `0` will be used as
   * the return value.
   */
  cancelId?: number;
}

export interface Certificate {
  data: string;
  fingerprint: string;
  issuer: CertificatePrincipal;
  issuerCert: Certificate;
  issuerName: string;
  serialNumber: string;
  subject: CertificatePrincipal;
  subjectName: string;
  validExpiry: number;
  validStart: number;
}

export interface CertificatePrincipal {
  commonName: string;
  country: string;
  locality: string;
  organizations: string[];
  organizationUnits: string[];
}

export interface CertificateTrustDialogOptions {
  certificate: Certificate;
  message: string;
}

export interface OpenDialogOptions {
  title?: string;
  defaultPath?: string;
  /**
   * Custom label for the confirmation button, when left empty the default label will
   * be used.
   */
  buttonLabel?: string;
  filters?: FileFilter[];
  /**
   * Contains which features the dialog should use. The following values are
   * supported:
   */
  properties?: Array<
    | 'openFile'
    | 'openDirectory'
    | 'multiSelections'
    | 'showHiddenFiles'
    | 'createDirectory'
    | 'promptToCreate'
    | 'noResolveAliases'
    | 'treatPackageAsDirectory'
    | 'dontAddToRecent'
  >;
  /**
   * Message to display above input boxes.
   *
   * @platform darwin
   */
  message?: string;
  /**
   * Create security scoped bookmarks when packaged for the Mac App Store.
   *
   * @platform darwin,mas
   */
  securityScopedBookmarks?: boolean;
}

export interface OpenDialogReturnValue {
  /**
   * whether or not the dialog was canceled.
   */
  canceled: boolean;
  /**
   * An array of file paths chosen by the user. If the dialog is cancelled this will
   * be an empty array.
   */
  filePaths: string[];
  /**
   * An array matching the `filePaths` array of base64 encoded strings which contains
   * security scoped bookmark data. `securityScopedBookmarks` must be enabled for
   * this to be populated. (For return values, see table here.)
   *
   * @platform darwin,mas
   */
  bookmarks?: string[];
}

export interface OpenDialogSyncOptions {
  title?: string;
  defaultPath?: string;
  /**
   * Custom label for the confirmation button, when left empty the default label will
   * be used.
   */
  buttonLabel?: string;
  filters?: FileFilter[];
  /**
   * Contains which features the dialog should use. The following values are
   * supported:
   */
  properties?: Array<
    | 'openFile'
    | 'openDirectory'
    | 'multiSelections'
    | 'showHiddenFiles'
    | 'createDirectory'
    | 'promptToCreate'
    | 'noResolveAliases'
    | 'treatPackageAsDirectory'
    | 'dontAddToRecent'
  >;
  /**
   * Message to display above input boxes.
   *
   * @platform darwin
   */
  message?: string;
  /**
   * Create security scoped bookmarks when packaged for the Mac App Store.
   *
   * @platform darwin,mas
   */
  securityScopedBookmarks?: boolean;
}

export interface SaveDialogOptions {
  /**
   * The dialog title. Cannot be displayed on some _Linux_ desktop environments.
   */
  title?: string;
  /**
   * Absolute directory path, absolute file path, or file name to use by default.
   */
  defaultPath?: string;
  /**
   * Custom label for the confirmation button, when left empty the default label will
   * be used.
   */
  buttonLabel?: string;
  filters?: FileFilter[];
  /**
   * Message to display above text fields.
   *
   * @platform darwin
   */
  message?: string;
  /**
   * Custom label for the text displayed in front of the filename text field.
   *
   * @platform darwin
   */
  nameFieldLabel?: string;
  /**
   * Show the tags input box, defaults to `true`.
   *
   * @platform darwin
   */
  showsTagField?: boolean;
  properties?: Array<
    | 'showHiddenFiles'
    | 'createDirectory'
    | 'treatPackageAsDirectory'
    | 'showOverwriteConfirmation'
    | 'dontAddToRecent'
  >;
  /**
   * Create a security scoped bookmark when packaged for the Mac App Store. If this
   * option is enabled and the file doesn't already exist a blank file will be
   * created at the chosen path.
   *
   * @platform darwin,mas
   */
  securityScopedBookmarks?: boolean;
}

export interface SaveDialogReturnValue {
  /**
   * whether or not the dialog was canceled.
   */
  canceled: boolean;
  /**
   * If the dialog is canceled, this will be an empty string.
   */
  filePath: string;
  /**
   * Base64 encoded string which contains the security scoped bookmark data for the
   * saved file. `securityScopedBookmarks` must be enabled for this to be present.
   * (For return values, see table here.)
   *
   * @platform darwin,mas
   */
  bookmark?: string;
}

export interface SaveDialogSyncOptions {
  /**
   * The dialog title. Cannot be displayed on some _Linux_ desktop environments.
   */
  title?: string;
  /**
   * Absolute directory path, absolute file path, or file name to use by default.
   */
  defaultPath?: string;
  /**
   * Custom label for the confirmation button, when left empty the default label will
   * be used.
   */
  buttonLabel?: string;
  filters?: FileFilter[];
  /**
   * Message to display above text fields.
   *
   * @platform darwin
   */
  message?: string;
  /**
   * Custom label for the text displayed in front of the filename text field.
   *
   * @platform darwin
   */
  nameFieldLabel?: string;
  /**
   * Show the tags input box, defaults to `true`.
   *
   * @platform darwin
   */
  showsTagField?: boolean;
  properties?: Array<
    | 'showHiddenFiles'
    | 'createDirectory'
    | 'treatPackageAsDirectory'
    | 'showOverwriteConfirmation'
    | 'dontAddToRecent'
  >;
  /**
   * Create a security scoped bookmark when packaged for the Mac App Store. If this
   * option is enabled and the file doesn't already exist a blank file will be
   * created at the chosen path.
   *
   * @platform darwin,mas
   */
  securityScopedBookmarks?: boolean;
}

export interface Dialog {
  // Docs: https://electronjs.org/docs/api/dialog

  /**
   * Displays a modal dialog that shows an error message.
   *
   * This API can be called safely before the `ready` event the `app` module emits,
   * it is usually used to report errors in early stage of startup. If called before
   * the app `ready`event on Linux, the message will be emitted to stderr, and no GUI
   * dialog will appear.
   */
  showErrorBox(title: string, content: string): void;
  /**
   * resolves with a promise containing the following properties:
   *
   * * `response` number - The index of the clicked button.
   * * `checkboxChecked` boolean - The checked state of the checkbox if
   * `checkboxLabel` was set. Otherwise `false`.
   *
   * Shows a message box.
   *
   * The `window` argument allows the dialog to attach itself to a parent window,
   * making it modal.
   */
  showMessageBox(
    window: BaseWindow,
    options: MessageBoxOptions
  ): Promise<MessageBoxReturnValue>;
  /**
   * resolves with a promise containing the following properties:
   *
   * * `response` number - The index of the clicked button.
   * * `checkboxChecked` boolean - The checked state of the checkbox if
   * `checkboxLabel` was set. Otherwise `false`.
   *
   * Shows a message box.
   *
   * The `window` argument allows the dialog to attach itself to a parent window,
   * making it modal.
   */
  showMessageBox(options: MessageBoxOptions): Promise<MessageBoxReturnValue>;
  /**
   * the index of the clicked button.
   *
   * Shows a message box, it will block the process until the message box is closed.
   * It returns the index of the clicked button.
   *
   * The `window` argument allows the dialog to attach itself to a parent window,
   * making it modal. If `window` is not shown dialog will not be attached to it. In
   * such case it will be displayed as an independent window.
   */
  showMessageBoxSync(
    window: BaseWindow,
    options: MessageBoxSyncOptions
  ): number;
  /**
   * the index of the clicked button.
   *
   * Shows a message box, it will block the process until the message box is closed.
   * It returns the index of the clicked button.
   *
   * The `window` argument allows the dialog to attach itself to a parent window,
   * making it modal. If `window` is not shown dialog will not be attached to it. In
   * such case it will be displayed as an independent window.
   */
  showMessageBoxSync(options: MessageBoxSyncOptions): number;
  /**
   * Resolve with an object containing the following:
   *
   * * `canceled` boolean - whether or not the dialog was canceled.
   * * `filePaths` string[] - An array of file paths chosen by the user. If the
   * dialog is cancelled this will be an empty array.
   * * `bookmarks` string[] (optional) _macOS_ _mas_ - An array matching the
   * `filePaths` array of base64 encoded strings which contains security scoped
   * bookmark data. `securityScopedBookmarks` must be enabled for this to be
   * populated. (For return values, see table here.)
   *
   * The `window` argument allows the dialog to attach itself to a parent window,
   * making it modal.
   *
   * The `filters` specifies an array of file types that can be displayed or selected
   * when you want to limit the user to a specific type. For example:
   *
   * The `extensions` array should contain extensions without wildcards or dots (e.g.
   * `'png'` is good but `'.png'` and `'*.png'` are bad). To show all files, use the
   * `'*'` wildcard (no other wildcard is supported).
   *
   * > [!NOTE] On Windows and Linux an open dialog can not be both a file selector
   * and a directory selector, so if you set `properties` to `['openFile',
   * 'openDirectory']` on these platforms, a directory selector will be shown.
   *
   * > [!NOTE] On Linux `defaultPath` is not supported when using portal file chooser
   * dialogs unless the portal backend is version 4 or higher. You can use
   * `--xdg-portal-required-version` command-line switch to force gtk or kde dialogs.
   */
  showOpenDialog(
    window: BaseWindow,
    options: OpenDialogOptions
  ): Promise<OpenDialogReturnValue>;
  /**
   * Resolve with an object containing the following:
   *
   * * `canceled` boolean - whether or not the dialog was canceled.
   * * `filePaths` string[] - An array of file paths chosen by the user. If the
   * dialog is cancelled this will be an empty array.
   * * `bookmarks` string[] (optional) _macOS_ _mas_ - An array matching the
   * `filePaths` array of base64 encoded strings which contains security scoped
   * bookmark data. `securityScopedBookmarks` must be enabled for this to be
   * populated. (For return values, see table here.)
   *
   * The `window` argument allows the dialog to attach itself to a parent window,
   * making it modal.
   *
   * The `filters` specifies an array of file types that can be displayed or selected
   * when you want to limit the user to a specific type. For example:
   *
   * The `extensions` array should contain extensions without wildcards or dots (e.g.
   * `'png'` is good but `'.png'` and `'*.png'` are bad). To show all files, use the
   * `'*'` wildcard (no other wildcard is supported).
   *
   * > [!NOTE] On Windows and Linux an open dialog can not be both a file selector
   * and a directory selector, so if you set `properties` to `['openFile',
   * 'openDirectory']` on these platforms, a directory selector will be shown.
   *
   * > [!NOTE] On Linux `defaultPath` is not supported when using portal file chooser
   * dialogs unless the portal backend is version 4 or higher. You can use
   * `--xdg-portal-required-version` command-line switch to force gtk or kde dialogs.
   */
  showOpenDialog(options: OpenDialogOptions): Promise<OpenDialogReturnValue>;
  /**
   * the file paths chosen by the user; if the dialog is cancelled it returns
   * `undefined`.
   *
   * The `window` argument allows the dialog to attach itself to a parent window,
   * making it modal.
   *
   * The `filters` specifies an array of file types that can be displayed or selected
   * when you want to limit the user to a specific type. For example:
   *
   * The `extensions` array should contain extensions without wildcards or dots (e.g.
   * `'png'` is good but `'.png'` and `'*.png'` are bad). To show all files, use the
   * `'*'` wildcard (no other wildcard is supported).
   *
   * > [!NOTE] On Windows and Linux an open dialog can not be both a file selector
   * and a directory selector, so if you set `properties` to `['openFile',
   * 'openDirectory']` on these platforms, a directory selector will be shown.
   *
   * > [!NOTE] On Linux `defaultPath` is not supported when using portal file chooser
   * dialogs unless the portal backend is version 4 or higher. You can use
   * `--xdg-portal-required-version` command-line switch to force gtk or kde dialogs.
   */
  showOpenDialogSync(
    window: BaseWindow,
    options: OpenDialogSyncOptions
  ): string[] | undefined;
  /**
   * the file paths chosen by the user; if the dialog is cancelled it returns
   * `undefined`.
   *
   * The `window` argument allows the dialog to attach itself to a parent window,
   * making it modal.
   *
   * The `filters` specifies an array of file types that can be displayed or selected
   * when you want to limit the user to a specific type. For example:
   *
   * The `extensions` array should contain extensions without wildcards or dots (e.g.
   * `'png'` is good but `'.png'` and `'*.png'` are bad). To show all files, use the
   * `'*'` wildcard (no other wildcard is supported).
   *
   * > [!NOTE] On Windows and Linux an open dialog can not be both a file selector
   * and a directory selector, so if you set `properties` to `['openFile',
   * 'openDirectory']` on these platforms, a directory selector will be shown.
   *
   * > [!NOTE] On Linux `defaultPath` is not supported when using portal file chooser
   * dialogs unless the portal backend is version 4 or higher. You can use
   * `--xdg-portal-required-version` command-line switch to force gtk or kde dialogs.
   */
  showOpenDialogSync(options: OpenDialogSyncOptions): string[] | undefined;
  /**
   * Resolve with an object containing the following:
   *
   * * `canceled` boolean - whether or not the dialog was canceled.
   * * `filePath` string - If the dialog is canceled, this will be an empty string.
   * * `bookmark` string (optional) _macOS_ _mas_ - Base64 encoded string which
   * contains the security scoped bookmark data for the saved file.
   * `securityScopedBookmarks` must be enabled for this to be present. (For return
   * values, see table here.)
   *
   * The `window` argument allows the dialog to attach itself to a parent window,
   * making it modal.
   *
   * The `filters` specifies an array of file types that can be displayed, see
   * `dialog.showOpenDialog` for an example.
   *
   * > [!NOTE] On macOS, using the asynchronous version is recommended to avoid
   * issues when expanding and collapsing the dialog.
   */
  showSaveDialog(
    window: BaseWindow,
    options: SaveDialogOptions
  ): Promise<SaveDialogReturnValue>;
  /**
   * Resolve with an object containing the following:
   *
   * * `canceled` boolean - whether or not the dialog was canceled.
   * * `filePath` string - If the dialog is canceled, this will be an empty string.
   * * `bookmark` string (optional) _macOS_ _mas_ - Base64 encoded string which
   * contains the security scoped bookmark data for the saved file.
   * `securityScopedBookmarks` must be enabled for this to be present. (For return
   * values, see table here.)
   *
   * The `window` argument allows the dialog to attach itself to a parent window,
   * making it modal.
   *
   * The `filters` specifies an array of file types that can be displayed, see
   * `dialog.showOpenDialog` for an example.
   *
   * > [!NOTE] On macOS, using the asynchronous version is recommended to avoid
   * issues when expanding and collapsing the dialog.
   */
  showSaveDialog(options: SaveDialogOptions): Promise<SaveDialogReturnValue>;
  /**
   * the path of the file chosen by the user; if the dialog is cancelled it returns
   * an empty string.
   *
   * The `window` argument allows the dialog to attach itself to a parent window,
   * making it modal.
   *
   * The `filters` specifies an array of file types that can be displayed, see
   * `dialog.showOpenDialog` for an example.
   */
  showSaveDialogSync(
    window: BaseWindow,
    options: SaveDialogSyncOptions
  ): string;
  /**
   * the path of the file chosen by the user; if the dialog is cancelled it returns
   * an empty string.
   *
   * The `window` argument allows the dialog to attach itself to a parent window,
   * making it modal.
   *
   * The `filters` specifies an array of file types that can be displayed, see
   * `dialog.showOpenDialog` for an example.
   */
  showSaveDialogSync(options: SaveDialogSyncOptions): string;
}

export declare const dialog: Dialog;
