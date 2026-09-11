import webview = require('@lynx-js/cef-webview');
import autolinkWebview = require('@lynx-js/cef-webview/lynxtron');

const initialized: boolean = webview.initialize();
const autolinkInitialized: boolean = autolinkWebview.initialize();
void [initialized, autolinkInitialized];
