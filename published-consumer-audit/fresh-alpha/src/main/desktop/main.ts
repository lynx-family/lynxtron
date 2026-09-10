import { app, LynxWindow } from '@lynx-js/lynxtron';
import cefWebview from '@lynx-js/cef-webview/lynxtron';
import { LYNX_BUNDLE_PATH } from './vendorPaths';
import path from 'node:path';
import os from 'node:os';
import fs from 'node:fs';
import http from 'node:http';
const auditPath = process.env.LYNXTRON_AUDIT_LOG || path.join(os.tmpdir(), 'fresh-alpha-audit.jsonl');
function audit(event: string, details: any = {}) {
  const line = JSON.stringify({ time: new Date().toISOString(), event, pid: process.pid, ...details });
  console.log(line);
  fs.appendFileSync(auditPath, line + '\n');
}
audit('HOST_STARTED', { cwd: process.cwd(), platform: process.platform, arch: process.arch, node: process.version, dev: process.env.NODE_ENV === 'development' });
const html = `<!doctype html><html><head><title>CEF FRESH ALPHA</title><style>body{margin:0;background:#087f5b;color:white;font-family:sans-serif;padding:36px}h1{font-size:38px}strong{display:block;background:#fff;color:#087f5b;padding:24px;font-size:30px}</style></head><body><h1>CEF FRESH ALPHA</h1><strong>PUBLIC NPM 0.0.21-alpha.main.1</strong><p id="status">PAGE LOADED</p><script>addEventListener('load',()=>{requestAnimationFrame(()=>requestAnimationFrame(()=>{document.querySelector('#status').textContent='CEF DOM + RAF READY';fetch('/cef-ready?title='+encodeURIComponent(document.title))}))})</script></body></html>`;
app.whenReady().then(() => {
  audit('APP_READY');
  const server = http.createServer((req, res) => {
    audit(req.url?.startsWith('/cef-ready') ? 'CEF_DOM_READY' : 'HTTP_REQUEST', { url: req.url, userAgent: req.headers['user-agent'] });
    res.setHeader('Content-Type', req.url?.startsWith('/cef-ready') ? 'text/plain' : 'text/html');
    res.end(req.url?.startsWith('/cef-ready') ? 'ok' : html);
  });
  server.on('error', error => audit('HTTP_ERROR', { error: String(error) }));
  server.listen(18769, '127.0.0.1', () => {
    audit('HTTP_LISTENING', { port: 18769 });
    const initialized = cefWebview.initialize();
    audit('CEF_INITIALIZED', { result: initialized });
    const w = new LynxWindow({ width: 1000, height: 760, title: 'Fresh Published Alpha CEF', lynxPreference: { preload: path.join(__dirname, 'preload.js') } });
    w.show();
    if (process.env.NODE_ENV === 'development') w.loadURL('http://localhost:5969/main.lynx.bundle');
    else w.loadFile(LYNX_BUNDLE_PATH);
    audit('WINDOW_LOAD_REQUESTED');
  });
});
