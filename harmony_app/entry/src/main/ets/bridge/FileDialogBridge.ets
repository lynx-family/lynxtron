import common from '@ohos.app.ability.common';
import fs from '@ohos.file.fs';
import fileUri from '@ohos.file.fileuri';
import hilog from '@ohos.hilog';
import picker from '@ohos.file.picker';
import lynxtron from 'liblynxtron_napi.so';
import { PersistManager } from './PersistManager';

const DOMAIN = 0x0000;
const TAG = 'FileDialogBridge';

/**
 * Bridges file_dialog::ShowOpenDialog (C++) to the OHOS system file picker.
 *
 * Electron reference: DialogAdapter.ets (showOpenDialog) + FilePickerAdapter
 * dirFilter + PermissionManagerAdapter fileAccessPersist.
 *
 * Division of labor:
 *   ArkTS: opens DocumentViewPicker, converts URI -> real path with
 *          `new fileUri.FileUri(uri).path` (same as Electron's
 *          FilePickerAdapter.dirFilter), passes PATHS to C++.
 *   C++:   FileAccessPersist (OH_FileShare_PersistPermission) persists file
 *          access on the ETS thread.
 *
 * NOTE: OH_FileUri_GetPathFromUri must NOT be called from liblynxtron.so —
 * its internal `new FileUri` crashes (SIGSEGV) because Chromium's allocator
 * shim intercepts the allocation.  URI->path conversion stays here in ArkTS.
 */
export class FileDialogBridge {
  private static instance: FileDialogBridge | null = null;
  private static MAX_SELECT_NUMBER: number = 500;
  private static DEFAULT_DIR: string = 'file://docs/storage/Users/currentUser';
  private ctx: common.UIAbilityContext;

  private constructor(ctx: common.UIAbilityContext) {
    this.ctx = ctx;
  }

  /**
   * Register the showOpenDialog handler with the lynxtron NAPI bridge.
   * Call via BridgeRegistry.registerAll from EntryAbility.onWindowStageCreate.
   */
  static register(context: common.UIAbilityContext): void {
    if (FileDialogBridge.instance) {
      hilog.warn(DOMAIN, TAG, 'Already registered, skipping');
      return;
    }
    FileDialogBridge.instance = new FileDialogBridge(context);
    const bridge = FileDialogBridge.instance;

    lynxtron.registerShowOpenDialog((id: number, settings: string) => {
      bridge.handleShowOpenDialog(id, settings);
    });
    lynxtron.registerShowSaveDialog((id: number, settings: string) => {
      bridge.handleShowSaveDialog(id, settings);
    });

    hilog.info(DOMAIN, TAG, 'Registered showOpenDialog/showSaveDialog handlers');
  }

  private handleShowOpenDialog(id: number, settingsJson: string): void {
    hilog.info(DOMAIN, TAG, 'handleShowOpenDialog id=%{public}d settings=%{public}s',
      id, settingsJson);
    let dialogSettings: IOpenDialogSettings = JSON.parse(settingsJson);
    let options = new picker.DocumentSelectOptions();

    if (dialogSettings.properties_open_directory) {
      options.selectMode = picker.DocumentSelectMode.FOLDER;
      if (dialogSettings.properties_open_mixed) {
        options.selectMode = picker.DocumentSelectMode.MIXED;
      }
    }
    if (dialogSettings.properties_multi_selections) {
      options.maxSelectNumber = FileDialogBridge.MAX_SELECT_NUMBER;
    }

    let allFileDescription = '所有文件';
    if (dialogSettings.filters.length > 0) {
      options.fileSuffixFilters = this.getFilterArray(dialogSettings.filters);
    } else {
      options.fileSuffixFilters = [allFileDescription + '|.*'];
    }

    let dirAndFile: string[] = this.handleDefaultPath(dialogSettings.default_path);
    options.defaultFilePathUri = dirAndFile[0];

    let documentPicker = new picker.DocumentViewPicker();
    documentPicker.select(options).then((uris: Array<string>) => {
      hilog.info(DOMAIN, TAG, 'select OK, uris=%{public}s',
        JSON.stringify(uris));
      if (uris.length === 0) {
        lynxtron.resolveShowOpenDialog(id, [], [], true);
        return;
      }
      // Pass BOTH: uris for C++ FileAccessPersist (must be file://docs URIs,
      // not paths), and paths for the JS-visible filePaths result.
      let paths: string[] = uris.map((uri) => new fileUri.FileUri(uri).path);
      hilog.info(DOMAIN, TAG, 'converted paths=%{public}s', JSON.stringify(paths));
      lynxtron.resolveShowOpenDialog(id, uris, paths, false);
      // [PERSIST] 选文件成功后, 把 URI 清单存起来, 供重启后激活(方案B)
      PersistManager.getInstance().saveUris(uris);
    }).catch((err: Error) => {
      hilog.error(DOMAIN, TAG, 'select failed: %{public}s', JSON.stringify(err));
      lynxtron.resolveShowOpenDialog(id, [], [], true);
    });
  }

  /**
   * Save dialog (showSaveDialog): DocumentSaveOptions + DocumentViewPicker.save().
   * Mirrors Electron DialogAdapter.ets showSaveDialog; URI->path conversion
   * happens here (C++ must not call OH_FileUri_GetPathFromUri — allocator shim
   * crash).  Passes raw URI (for persist) + converted path to C++.
   */
  private handleShowSaveDialog(id: number, settingsJson: string): void {
    hilog.info(DOMAIN, TAG, 'handleShowSaveDialog id=%{public}d settings=%{public}s',
      id, settingsJson);
    let dialogSettings: IOpenDialogSettings = JSON.parse(settingsJson);
    let options = new picker.DocumentSaveOptions();

    let dirAndFile: string[] = this.handleDefaultPath(dialogSettings.default_path);
    hilog.info(DOMAIN, TAG, 'saveDialog dirAndFile=%{public}s|%{public}s',
      dirAndFile[0], dirAndFile[1]);
    options.defaultFilePathUri = dirAndFile[0];
    if (dirAndFile[1].length > 0) {
      options.newFileNames = [dirAndFile[1]];
    }

    let allFileDescription = '所有文件';
    if (dialogSettings.filters.length > 0) {
      options.fileSuffixChoices = this.getFilterArray(dialogSettings.filters);
    } else {
      options.fileSuffixChoices = [allFileDescription + '|.*'];
    }

    let documentPicker = new picker.DocumentViewPicker();
    documentPicker.save(options).then((saveResult: Array<string>) => {
      hilog.info(DOMAIN, TAG, 'save OK, result=%{public}s',
        JSON.stringify(saveResult));
      if (saveResult.length === 0) {
        lynxtron.resolveShowSaveDialog(id, '', '', true);
        return;
      }
      let uri = saveResult[0];
      let path = new fileUri.FileUri(uri).path;
      hilog.info(DOMAIN, TAG, 'saved uri=%{public}s path=%{public}s', uri, path);
      lynxtron.resolveShowSaveDialog(id, uri, path, false);
    }).catch((err: Error) => {
      hilog.error(DOMAIN, TAG, 'save failed: %{public}s', JSON.stringify(err));
      lynxtron.resolveShowSaveDialog(id, '', '', true);
    });
  }

  private handleDefaultPath(defaultPath: string): string[] {
    let dirAndFile: string[] = [FileDialogBridge.DEFAULT_DIR, ''];
    try {
      let path: string = defaultPath.trim();
      const prefix = 'file://docs/';
      const isFileUri = path.startsWith(prefix);
      const protocolPrefix = isFileUri ? prefix : '';
      let pathWithoutProtocol = isFileUri ? path.substring(prefix.length) : path;
      let displayPath = decodeURIComponent(pathWithoutProtocol);
      // fs 需要绝对路径：URI 去掉 file://docs/ 后缺前导斜杠，补上
      // (Electron 原版用相对路径 accessSync，非空 defaultPath 时必然失败——bug 修复)
      let fsPath = displayPath.startsWith('/') ? displayPath : '/' + displayPath;
      let exist = fs.accessSync(fsPath);
      if (exist) {
        const lastSlashIdx = fsPath.lastIndexOf('/');
        const dirPart = fsPath.substring(0, lastSlashIdx);
        const targetPartUri = fsPath.substring(lastSlashIdx + 1);
        let stat = fs.statSync(fsPath);
        if (stat.isDirectory()) {
          dirAndFile[0] = protocolPrefix + dirPart.substring(1) + '/' + targetPartUri;
          dirAndFile[1] = '';
        } else if (stat.isFile()) {
          dirAndFile[0] = protocolPrefix + dirPart.substring(1);
          dirAndFile[1] = targetPartUri;
        }
      } else {
        // save 场景: defaultPath 是"将要创建的文件", 文件本身不存在,
        // 但父目录应该存在 —— 拆出文件名, 用父目录作为 defaultFilePathUri
        const lastSlashIdx = fsPath.lastIndexOf('/');
        if (lastSlashIdx > 0) {
          const parentFs = fsPath.substring(0, lastSlashIdx);
          const fileName = fsPath.substring(lastSlashIdx + 1);
          if (fileName.length > 0 && fs.accessSync(parentFs)) {
            dirAndFile[0] = protocolPrefix + parentFs.substring(1);
            dirAndFile[1] = fileName;
            return dirAndFile;
          }
        }
        hilog.error(DOMAIN, TAG, 'accessSync failed, path not exist: %{public}s',
          fsPath);
      }
    } catch (err) {
      hilog.error(DOMAIN, TAG, 'handleDefaultPath failed: %{public}s',
        JSON.stringify(err));
    }
    return dirAndFile;
  }

  private getFilterArray(filters: IFilter[]): Array<string> {
    let copy = new Array<string>();
    for (const filter of filters) {
      copy.push(filter.name + '(*.' + filter.extensions.join(';*.') + ')|.' +
        filter.extensions.join(',.'));
    }
    return copy;
  }
}

interface IFilter {
  name: string;
  extensions: string[];
}

interface IOpenDialogSettings {
  parent_id: number;
  title: string;
  message: string;
  button_label: string;
  default_path: string;
  filters: IFilter[];
  properties_open_directory: boolean;
  properties_open_mixed: boolean;
  properties_multi_selections: boolean;
}
