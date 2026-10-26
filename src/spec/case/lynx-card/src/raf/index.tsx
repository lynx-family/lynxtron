// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { root, useEffect } from '@lynx-js/react';

function App() {
  useEffect(() => {
    const id = requestAnimationFrame(() => {
      (NativeModules as any).bridge.send('raf-callback', {});
    });
    return () => cancelAnimationFrame(id);
  }, []);

  return <text>RAF callback test</text>;
}

root.render(<App />);
