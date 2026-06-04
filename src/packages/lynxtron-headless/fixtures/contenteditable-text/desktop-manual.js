import { createRequire } from 'module';
import path from 'path';
import { fileURLToPath } from 'url';

const requireLynxtron = createRequire(
  'file:///Users/bytedance/ws2/lynxtron_oss_ws/lynxtron/src/packages/lynxtron/lynxtron.js'
);
const { app, LynxWindow } = requireLynxtron('lynxtron');

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const bundlePath = path.join(__dirname, 'dist', 'headless_contenteditable_text.bundle');

app.whenReady().then(() => {
  const w = new LynxWindow({
    width: 760,
    height: 920,
    title: 'Contenteditable Text Lab',
  });

  w.show();
  w.loadFile(bundlePath);
});
