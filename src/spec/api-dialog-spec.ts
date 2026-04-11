// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { LynxWindow, app } from 'lynxtron';

import { expect } from 'chai';

import * as path from 'node:path';

import { ifdescribe, ifit, waitUntil } from './lib/spec-helpers';
import { closeAllWindows } from './lib/window-helpers';

const nativeDialog = (require('lynxtron') as typeof import('lynxtron') & {
  dialog: typeof import('../lib/browser/api/dialog');
}).dialog;

describe('dialog module', () => {
  const dialogModulePath = require.resolve('../lib/browser/api/dialog');
  const originalLinkedBinding = (process as any)._linkedBinding;

  let dialog: typeof import('../lib/browser/api/dialog');
  let binding: Record<string, any>;
  let lastOpenDialogSettings: any;
  let lastSaveDialogSettings: any;
  let lastMessageBoxSettings: any;
  let closedMessageBoxIds: number[];
  let resolveMessageBox:
    | ((result: { response: number; checkboxChecked: boolean }) => void)
    | undefined;

  const loadDialogModule = () => {
    delete require.cache[dialogModulePath];
    dialog = require('../lib/browser/api/dialog');
  };

  const createDialogBinding = () => ({
    showOpenDialogSync(settings: any) {
      lastOpenDialogSettings = settings;
      return undefined;
    },
    showOpenDialog(settings: any) {
      lastOpenDialogSettings = settings;
      return Promise.resolve({ canceled: true, filePaths: [] });
    },
    showSaveDialogSync(settings: any) {
      lastSaveDialogSettings = settings;
      return undefined;
    },
    showSaveDialog(settings: any) {
      lastSaveDialogSettings = settings;
      return Promise.resolve({ canceled: true, filePath: '' });
    },
    showMessageBoxSync(settings: any) {
      lastMessageBoxSettings = settings;
      return settings.cancelId ?? 0;
    },
    showMessageBox(settings: any) {
      lastMessageBoxSettings = settings;
      return Promise.resolve({
        response: settings.cancelId ?? 0,
        checkboxChecked: settings.checkboxChecked ?? false,
      });
    },
    _closeMessageBox(id: number) {
      closedMessageBoxIds.push(id);
      if (resolveMessageBox) {
        const resolve = resolveMessageBox;
        resolveMessageBox = undefined;
        resolve({
          response: lastMessageBoxSettings.cancelId ?? 0,
          checkboxChecked: lastMessageBoxSettings.checkboxChecked ?? false,
        });
      }
    },
    showErrorBox() {},
    showCertificateTrustDialog() {},
  });

  beforeEach(() => {
    lastOpenDialogSettings = undefined;
    lastSaveDialogSettings = undefined;
    lastMessageBoxSettings = undefined;
    closedMessageBoxIds = [];
    resolveMessageBox = undefined;

    binding = createDialogBinding();
    (process as any)._linkedBinding = (name: string) => {
      if (name === 'lynxtron_binding_dialog') {
        return binding;
      }
      return originalLinkedBinding.call(process, name);
    };

    loadDialogModule();
  });

  afterEach(async () => {
    (process as any)._linkedBinding = originalLinkedBinding;
    delete require.cache[dialogModulePath];
    await closeAllWindows();
  });

  describe('showOpenDialog', () => {
    it('throws errors when the options are invalid', () => {
      expect(() => {
        dialog.showOpenDialog({ properties: false as any });
      }).to.throw(/Properties must be an array/);

      expect(() => {
        dialog.showOpenDialog({ title: 300 as any });
      }).to.throw(/Title must be a string/);

      expect(() => {
        dialog.showOpenDialog({ buttonLabel: [] as any });
      }).to.throw(/Button label must be a string/);

      expect(() => {
        dialog.showOpenDialog({ defaultPath: {} as any });
      }).to.throw(/Default path must be a string/);

      expect(() => {
        dialog.showOpenDialog({ message: {} as any });
      }).to.throw(/Message must be a string/);
    });

    it('passes normalized properties through to the binding', async () => {
      await dialog.showOpenDialog({
        title: 'Open',
        properties: ['openFile', 'multiSelections'],
      });

      expect(lastOpenDialogSettings.title).to.equal('Open');
      expect(lastOpenDialogSettings.properties).to.equal((1 << 0) | (1 << 2));
    });
  });

  describe('showSaveDialog', () => {
    it('throws errors when the options are invalid', () => {
      expect(() => {
        dialog.showSaveDialog({ title: 300 as any });
      }).to.throw(/Title must be a string/);

      expect(() => {
        dialog.showSaveDialog({ buttonLabel: [] as any });
      }).to.throw(/Button label must be a string/);

      expect(() => {
        dialog.showSaveDialog({ defaultPath: {} as any });
      }).to.throw(/Default path must be a string/);

      expect(() => {
        dialog.showSaveDialog({ message: {} as any });
      }).to.throw(/Message must be a string/);

      expect(() => {
        dialog.showSaveDialog({ nameFieldLabel: {} as any });
      }).to.throw(/Name field label must be a string/);
    });

    it('passes normalized properties through to the binding', async () => {
      await dialog.showSaveDialog({
        title: 'Save',
        properties: ['showOverwriteConfirmation', 'dontAddToRecent'],
      });

      expect(lastSaveDialogSettings.title).to.equal('Save');
      expect(lastSaveDialogSettings.properties).to.equal((1 << 3) | (1 << 4));
    });
  });

  describe('showMessageBox', () => {
    it('throws errors when the options are invalid', () => {
      expect(() => {
        dialog.showMessageBox(undefined as any, {
          type: 'not-a-valid-type' as any,
          message: '',
        });
      }).to.throw(/Invalid message box type/);

      expect(() => {
        dialog.showMessageBox(null as any, {
          buttons: false as any,
          message: '',
        });
      }).to.throw(/Buttons must be an array/);

      expect(() => {
        dialog.showMessageBox({ title: 300 as any, message: '' });
      }).to.throw(/Title must be a string/);

      expect(() => {
        dialog.showMessageBox({ message: [] as any });
      }).to.throw(/Message must be a string/);

      expect(() => {
        dialog.showMessageBox({ detail: 3.14 as any, message: '' });
      }).to.throw(/Detail must be a string/);

      expect(() => {
        dialog.showMessageBox({ checkboxLabel: false as any, message: '' });
      }).to.throw(/checkboxLabel must be a string/);
    });

    it('throws when checkboxChecked is set without checkboxLabel', () => {
      expect(() => {
        dialog.showMessageBox({
          message: 'Checkbox test',
          checkboxChecked: true,
        });
      }).to.throw(/checkboxChecked requires that checkboxLabel also be passed/);
    });

    it('infers cancelId from a "Cancel" button label', async () => {
      await dialog.showMessageBox({
        message: 'Cancel test',
        buttons: ['OK', 'Cancel'],
      });

      expect(lastMessageBoxSettings.cancelId).to.equal(1);
    });

    it('infers cancelId from a "No" button label', async () => {
      await dialog.showMessageBox({
        message: 'No test',
        buttons: ['Yes', 'No'],
      });

      expect(lastMessageBoxSettings.cancelId).to.equal(1);
    });

    it('falls back to the second button when defaultId is 0', async () => {
      await dialog.showMessageBox({
        message: 'Default button test',
        buttons: ['One', 'Two'],
        defaultId: 0,
      });

      expect(lastMessageBoxSettings.cancelId).to.equal(1);
    });

    ifit(process.platform === 'darwin')(
      'strips access keys on macOS with normalizeAccessKeys',
      async () => {
        await dialog.showMessageBox({
          message: 'Access key test',
          buttons: ['&Save', 'Save && Close'],
          normalizeAccessKeys: true,
        });

        expect(lastMessageBoxSettings.buttons).to.deep.equal([
          'Save',
          'Save & Close',
        ]);
      }
    );
  });

  describe('showMessageBox with signal', () => {
    beforeEach(() => {
      binding.showMessageBox = (settings: any) => {
        lastMessageBoxSettings = settings;
        return new Promise((resolve) => {
          resolveMessageBox = resolve;
        });
      };
    });

    it('closes message box immediately', async () => {
      const controller = new AbortController();
      const promise = dialog.showMessageBox({
        signal: controller.signal,
        message: 'i am message',
      });

      controller.abort();
      const result = await promise;

      expect(closedMessageBoxIds).to.deep.equal([lastMessageBoxSettings.id]);
      expect(result.response).to.equal(0);
    });

    it('does not crash when there is a defaultId but no buttons', async () => {
      const controller = new AbortController();
      const promise = dialog.showMessageBox({
        signal: controller.signal,
        message: 'i am message',
        type: 'info',
        defaultId: 0,
        title: 'i am title',
      });

      controller.abort();
      const result = await promise;

      expect(closedMessageBoxIds).to.deep.equal([lastMessageBoxSettings.id]);
      expect(result.response).to.equal(0);
    });

    it('returns cancelId when the dialog is aborted', async () => {
      const controller = new AbortController();
      const promise = dialog.showMessageBox({
        signal: controller.signal,
        message: 'i am message',
        buttons: ['OK', 'Cancel'],
        cancelId: 1,
      });

      controller.abort();
      const result = await promise;

      expect(closedMessageBoxIds).to.deep.equal([lastMessageBoxSettings.id]);
      expect(result.response).to.equal(1);
    });
  });

  describe('showCertificateTrustDialog', () => {
    it('throws errors when the options are invalid', () => {
      expect(() => {
        (dialog.showCertificateTrustDialog as any)();
      }).to.throw(/options must be an object/);

      expect(() => {
        dialog.showCertificateTrustDialog({} as any);
      }).to.throw(/certificate must be an object/);

      expect(() => {
        dialog.showCertificateTrustDialog({
          certificate: {} as any,
          message: false as any,
        });
      }).to.throw(/message must be a string/);
    });
  });
});

ifdescribe(
  process.platform === 'darwin' &&
    !process.env.ELECTRON_SKIP_NATIVE_MODULE_TESTS
)('dialog module end-to-end interaction (macOS)', () => {
  let dialogHelper: any;

  before(() => {
    dialogHelper = require('@lynxtron-ci/dialog-helper');
  });

  afterEach(closeAllWindows);

  const waitForSheet = async (window: LynxWindow) => {
    const handle = window.getNativeWindowHandle();
    await waitUntil(() => dialogHelper.getDialogInfo(handle).type !== 'none', {
      rate: 100,
      timeout: 5000,
    });
  };

  describe('showMessageBox', () => {
    it('shows the expected message and buttons', async () => {
      const window = new LynxWindow({ show: false });
      const promise = nativeDialog.showMessageBox(window, {
        message: 'Test message',
        buttons: ['OK', 'Cancel'],
      });

      await waitForSheet(window);
      const handle = window.getNativeWindowHandle();
      const info = dialogHelper.getDialogInfo(handle);

      expect(info.type).to.equal('message-box');
      expect(info.message).to.equal('Test message');
      expect(JSON.parse(info.buttons)).to.deep.equal(['OK', 'Cancel']);

      dialogHelper.clickMessageBoxButton(handle, 1);
      const result = await promise;
      expect(result.response).to.equal(1);
    });

    it('toggles the checkbox and returns the updated state', async () => {
      const window = new LynxWindow({ show: false });
      const promise = nativeDialog.showMessageBox(window, {
        message: 'Checkbox test',
        buttons: ['OK'],
        checkboxLabel: 'Remember my choice',
        checkboxChecked: false,
      });

      await waitForSheet(window);
      const handle = window.getNativeWindowHandle();

      let info = dialogHelper.getDialogInfo(handle);
      expect(info.checkboxLabel).to.equal('Remember my choice');
      expect(info.checkboxChecked).to.be.false();

      expect(dialogHelper.clickCheckbox(handle)).to.be.true();
      info = dialogHelper.getDialogInfo(handle);
      expect(info.checkboxChecked).to.be.true();

      dialogHelper.clickMessageBoxButton(handle, 0);
      const result = await promise;
      expect(result.checkboxChecked).to.be.true();
    });
  });

  describe('showOpenDialog', () => {
    it('exposes open dialog state through the helper and can be cancelled', async () => {
      const window = new LynxWindow({ show: false });
      const promise = nativeDialog.showOpenDialog(window, {
        buttonLabel: 'Select This',
        message: 'Choose a file to import',
        properties: ['openFile', 'multiSelections', 'showHiddenFiles'],
      });

      await waitForSheet(window);
      const handle = window.getNativeWindowHandle();
      const info = dialogHelper.getDialogInfo(handle);

      expect(info.type).to.equal('open-dialog');
      expect(info.prompt).to.equal('Select This');
      expect(info.panelMessage).to.equal('Choose a file to import');
      expect(info.canChooseFiles).to.be.true();
      expect(info.canChooseDirectories).to.be.false();
      expect(info.allowsMultipleSelection).to.be.true();
      expect(info.showsHiddenFiles).to.be.true();

      expect(dialogHelper.cancelFileDialog(handle)).to.be.true();
      const result = await promise;
      expect(result.canceled).to.be.true();
      expect(result.filePaths).to.deep.equal([]);
    });
  });

  describe('showSaveDialog', () => {
    it('exposes save dialog state through the helper and can be accepted', async () => {
      const defaultPath = path.join(
        app.getPath('temp'),
        'lynxtron-dialog-helper-save.txt'
      );
      const window = new LynxWindow({ show: false });
      const promise = nativeDialog.showSaveDialog(window, {
        buttonLabel: 'Export',
        message: 'Choose where to save',
        nameFieldLabel: 'Export As:',
        defaultPath,
        showsTagField: false,
      });

      await waitForSheet(window);
      const handle = window.getNativeWindowHandle();
      const info = dialogHelper.getDialogInfo(handle);

      expect(info.type).to.equal('save-dialog');
      expect(info.prompt).to.equal('Export');
      expect(info.panelMessage).to.equal('Choose where to save');
      expect(info.nameFieldLabel).to.equal('Export As:');
      expect(info.nameFieldValue).to.equal(path.basename(defaultPath));
      expect(info.directory).to.equal(path.dirname(defaultPath));
      expect(info.showsTagField).to.be.false();

      expect(dialogHelper.acceptFileDialog(handle)).to.be.true();
      const result = await promise;
      expect(result.canceled).to.be.false();
      expect(result.filePath).to.equal(defaultPath);
    });
  });
});
