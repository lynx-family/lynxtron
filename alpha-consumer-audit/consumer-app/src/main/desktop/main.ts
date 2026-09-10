import { app, LynxWindow } from '@lynx-js/lynxtron';
import { LYNX_BUNDLE_PATH } from './vendorPaths';
import path from 'node:path';
import http from 'node:http';
import cefWebview from '@lynx-js/cef-webview/lynxtron';
import widget from '@alpha-consumer/widget/lynxtron';
const isDev = process.env.NODE_ENV === 'development';

app.whenReady().then(() => {
  console.log('[ALPHA_AUDIT] HOST_READY');
  widget.initialize();
  console.log('[ALPHA_AUDIT] WIDGET_INITIALIZED');
  cefWebview.initialize();
  console.log('[ALPHA_AUDIT] CEF_INITIALIZED');
  const server = http.createServer((_request, response) => {
    response.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
    response.end('<!doctype html><title>ALPHA_CEF_100</title><h1>ALPHA_CEF_100</h1><p>CEF WebView local page loaded.</p>');
    console.log('[ALPHA_AUDIT] CEF_HTTP_REQUEST');
  });
  server.listen(17981, '127.0.0.1');
  app.on('before-quit', () => server.close());
  const w = new LynxWindow({
    width: 800,
    height: 600,
    title: 'Alpha Consumer 100',
    lynxPreference: { preload: path.join(__dirname, 'preload.js') },
  });
  w.on('-lynx-invoke', (callback: any, name: string, data: unknown) => {
    if (name === 'audit') {
      console.log('[ALPHA_AUDIT]', JSON.stringify(data));
      callback.sendReply('ok');
    }
  });
  w.show();
  if (isDev) w.loadURL('http://localhost:5969/main.lynx.bundle');
  else w.loadFile(LYNX_BUNDLE_PATH);
});
