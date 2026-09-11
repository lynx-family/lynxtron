import webview from '@lynx-js/cef-webview';
import autolinkWebview from '@lynx-js/cef-webview/lynxtron';

const initialized: boolean = webview.initialize();
const autolinkInitialized: boolean = autolinkWebview.initialize();
void [initialized, autolinkInitialized];

// @ts-expect-error Initialization does not return a string.
const invalid: string = webview.initialize();
void invalid;
