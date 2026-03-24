// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { useCallback } from '@lynx-js/react';
import './App.css';
import placeholder from '@assets/placeholder.png?inline';

export function App() {
  const handleTap = useCallback(() => {
    console.log('[App] handleTap triggered', NativeModules.nodejs.getExposed());
    NativeModules.bridge.call(
      'showDialog',
      {
        message: NativeModules.nodejs.getExposed().echo('Hello from Lynxtron!'),
      },
      () => {
        console.log('[App] bridge.request callback fired');
      }
    );
  }, []);

  return (
    <view className="Background">
      <image className="BackgroundImage" src={placeholder}></image>
      <view className="Container" bindtap={handleTap}>
        <text className="Title">Hello, Lynxtron</text>
        <text className="Hint">Tap card to show native dialog</text>
      </view>
    </view>
  );
}
