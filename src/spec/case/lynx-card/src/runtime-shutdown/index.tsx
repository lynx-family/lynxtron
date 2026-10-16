// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { root, useEffect } from '@lynx-js/react';

function App() {
  useEffect(() => {
    const id = String((lynx.__globalProps as { windowId: number }).windowId);
    const api = (NativeModules as any).nodejs.exposed;
    api.mark('ready', id);
    (NativeModules as any).bridge.send('shutdown-ready', id);
    return () => {
      // Exercise tiering/worker-task submission while the destroy lifetime runs.
      function hotFunction(value: number) {
        return Math.imul(value ^ 0x1234, 31) >>> 0;
      }
      let checksum = 0;
      for (let i = 0; i < 1000000; ++i) {
        checksum = hotFunction(checksum + i);
      }
      api.mark('destroyed', id, checksum);
    };
  }, []);
  return (
    <view>
      <text>Runtime shutdown regression</text>
    </view>
  );
}

root.render(<App />);
