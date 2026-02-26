import { app, LynxWindow } from '@lynx-js/lynxtron';
import { LYNX_BUNDLE_PATH } from './vendorPaths';
const isDev = process.env.NODE_ENV === 'development';

console.log('start......');

app.whenReady().then(() => {
  console.log('app version: ', app.getVersion());

  const w = new LynxWindow({
    width: 800,
    height: 600,
  });

  w.on('show', () => {
    console.log('window is shown');
  });
  w.on('blur', () => {
    console.log('window is blurred');
  });

  w.show();
  console.log('LYNX_BUNDLE_PATH: ', LYNX_BUNDLE_PATH);

  if (isDev) {
    // TODO(zhengsenyao): Use loadURL to load dev bundle
    // w.loadURL('http://localhost:3000/main.lynx.bundle')
    w.loadFile(LYNX_BUNDLE_PATH);
  } else {
    w.loadFile(LYNX_BUNDLE_PATH);
  }
});
