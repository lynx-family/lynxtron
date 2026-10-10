// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { useEffect } from '@lynx-js/react';

export default function App() {
  useEffect(() => {
    const emitter = lynx.getJSModule('GlobalEventEmitter') as {
      addListener(eventName: string, listener: () => void): void;
    };
    emitter.addListener('keyboardstatuschanged', () => {});

    const bridge = (NativeModules as any).bridge as any;
    bridge.send('lynx-set-module-ready', { registered: true });
  }, []);

  return (
    <view style={{ flexDirection: 'column' as const }} className="container">
      <text>LynxSetModule compatibility</text>
    </view>
  );
}
