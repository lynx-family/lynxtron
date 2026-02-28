/**
 * Lynxtron Web Host Infrastructure
 * Provides NativeModule simulation for Lynx running in Browser.
 */

/**
 * Creates a JS source string for the Native Module proxy (RPC mode).
 */
function createProxyCode(moduleName, methods) {
  const methodStrings = methods.map(method => `
    async ${method}(...args) {
      const callback = typeof args[args.length - 1] === 'function' ? args.pop() : null;
      const res = await NativeModulesCall('${moduleName}:${method}', args);
      if (typeof callback === 'function') {
        callback(res);
      }
    }`).join(',');

  return `
    export default function(NativeModules, NativeModulesCall) {
      return {
        ${methodStrings}
      };
    };
  `;
}

/**
 * Setup Symmetric Host for Web.
 * @param {any} lynxView The <lynx-view> element.
 * @param {object} config Configuration for bridge and nodejs.
 */
export function setupSymmetricHost(lynxView, config = {}) {
  const nativeModulesMap = {};
  const handlers = {};

  // 1. Setup Main Thread Bridge (bridge) - RPC Mode
  if (config.bridge) {
    const bridgeMethods = Object.keys(config.bridge);
    const code = createProxyCode('bridge', bridgeMethods);
    const blob = new Blob([code], { type: 'text/javascript' });
    nativeModulesMap['bridge'] = URL.createObjectURL(blob);

    for (const [methodName, handler] of Object.entries(config.bridge)) {
      handlers[`bridge:${methodName}`] = handler;
    }
  }

  // 2. Setup Background Service (nodejs) - Direct Injection Mode
  // In Lynxtron PC, NativeModules.nodejs runs in the same Background Thread.
  // In Web, we achieve this by importScripts() inside the Proxy Blob.
  if (config.nodejs && config.nodejs.scriptURL) {
    const { scriptURL } = config.nodejs;
    // Resolve relative URL to absolute URL in main thread
    const absoluteScriptURL = new URL(scriptURL, window.location.href).href;
    console.log('[Lynxtron Web] Configuring nodejs module with script:', absoluteScriptURL);
    
    const code = `
      export default function(NativeModules, NativeModulesCall) {
        console.log('[Lynxtron Web] Initializing nodejs module proxy in context:', self.constructor.name);
        try {
          console.log('[Lynxtron Web] Loading script:', '${absoluteScriptURL}');
          
          // Polyfill contextBridge for Web Adapter
          self.contextBridge = {
            exposeInLynxBTS: (exports) => {
              self.__lynxtron_nodejs_exports__ = exports;
            }
          };

          // Load the logic script into Lynx Background Worker
          importScripts('${absoluteScriptURL}');
          
          // Retrieve capabilities from global registry
          const exports = self.__lynxtron_nodejs_exports__ || {};
          console.log('[Lynxtron Web] Discovered exports:',exports);
          
          return {
            getExposed: () => exports,
          };
        } catch (err) {
          console.error('[Lynxtron Web] Failed to inject nodejs module:', err);
          return {};
        }
      };
    `;
    
    const blob = new Blob([code], { type: 'text/javascript' });
    const blobUrl = URL.createObjectURL(blob);
    console.log('[Lynxtron Web] Created Proxy Blob URL:', blobUrl);
    nativeModulesMap['nodejs'] = blobUrl;
  }

  // 3. Inject to Lynx
  console.log('[Lynxtron Web] Final nativeModulesMap:', nativeModulesMap);
  lynxView.nativeModulesMap = nativeModulesMap;
  lynxView.onNativeModulesCall = async (name, args) => {
    const handler = handlers[name];
    if (handler) {
      // If args is an array, we spread it. If it's single data (legacy), we pass it as is.
      // But based on our new proxy, it will always be an array of arguments.
      if (Array.isArray(args)) {
        return await handler(...args);
      }
      return await handler(args);
    }
    console.warn(`[Lynxtron Web] No handler for: ${name}`);
    return null;
  };
}
