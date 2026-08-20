import { app, Notification } from '@lynx-js/lynxtron';

const expected = process.platform === 'darwin' || process.platform === 'win32';
const actual = Notification.isSupported();

console.log(
  JSON.stringify({
    notificationType: typeof Notification,
    isSupportedType: typeof Notification.isSupported,
    expected,
    actual,
  }),
);

app.whenReady().then(() => {
  app.exit(actual === expected ? 0 : 1);
});
