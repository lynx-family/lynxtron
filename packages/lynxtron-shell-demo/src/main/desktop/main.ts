import { app, LynxWindow, dialog } from '@lynx-js/lynxtron';
import { LYNX_BUNDLE_PATH } from './vendorPaths';
import path from 'path';

app.whenReady().then(() => {
  const w = new LynxWindow({
    width: 800,
    height: 600,
    title: 'Lynxtron Hello World',
    nodeIntegration: {
      preload_paths: [path.join(__dirname, 'preload.js')],
    },
  });

  // Handle bridge calls from Lynx UI
  // @ts-ignore
  w.on(
    '-lynx-invoke',
    async (callback: EventCallback, name: string, data: any) => {
      // In our architecture, UI calls NativeModules.bridge.request({ method, params })
      console.log(
        `[PC_Host] NativeModule Call: bridge.${name}`,
        data,
        callback,
        name
      );

      if (name === 'showDialog') {
        const { message } = data;
        dialog.showMessageBox({ message });
        callback.sendReply();
      } else if (name == 'getAppVersion') {
        callback.sendReply(app.getVersion());
      }
    }
  );

  w.show();
  w.loadFile(LYNX_BUNDLE_PATH);
});
