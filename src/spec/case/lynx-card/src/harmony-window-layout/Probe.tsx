// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { useEffect } from '@lynx-js/react';
import App from './App';

declare const SystemInfo: { pixelRatio: number };

// Original lynxtron-B / Windows welcome layout, unchanged apart from test ids.
// Report logical geometry without adding visible controls to the layout.
export function WindowLayoutProbe() {
  useEffect(() => {
    const sample = () => {
      for (const id of [
        'root',
        'inner',
        'introduction',
        'grid',
        'card-0',
        'card-1',
        'card-2',
      ]) {
        lynx
          .createSelectorQuery()
          .select(`#window-layout-${id}`)
          .invoke({
            method: 'boundingClientRect',
            params: { relativeTo: '#window-layout-root' },
            success: (rect: any) =>
              console.log(
                '[WINDOW-LAYOUT]',
                JSON.stringify({
                  id,
                  dpr: SystemInfo.pixelRatio,
                  width: rect.width,
                  height: rect.height,
                  x: rect.left,
                  y: rect.top,
                })
              ),
            fail: () => console.log('[WINDOW-LAYOUT] query failed', id),
          })
          .exec();
      }
    };
    sample();
    const timer = setInterval(sample, 2000);
    return () => clearInterval(timer);
  }, []);
  return <App />;
}
