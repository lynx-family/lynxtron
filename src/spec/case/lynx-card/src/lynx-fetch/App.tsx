// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { useEffect } from '@lynx-js/react';

export default function App() {
  useEffect(() => {
    const emitter = lynx.getJSModule('GlobalEventEmitter') as any;
    const bridge = (NativeModules as any).bridge as any;
    const handler = async (params: { url: string }) => {
      try {
        const response = await fetch(params.url, {
          method: 'POST',
          headers: { 'x-lynx-fetch': 'request-header' },
          body: 'request-body',
        });
        bridge.send('fetch_result', {
          status: response.status,
          responseHeader: response.headers.get('x-lynx-fetch'),
        });
      } catch (error) {
        bridge.send('fetch_result', {
          error: error instanceof Error ? error.message : String(error),
        });
      }
    };

    const subscription = emitter.addListener('fetch_request', handler, lynx);
    return () => subscription?.remove?.();
  }, []);

  return (
    <view style={{ flexDirection: 'column' as const }} className="container">
      <text>Lynx fetch test</text>
    </view>
  );
}
