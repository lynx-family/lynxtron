// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import {
  useEffect,
  useInitData,
  useLynxGlobalEventListener,
  useState,
} from '@lynx-js/react';

type GlobalProps = {
  appTheme?: unknown;
  flags?: {
    initial?: unknown;
    replacement?: unknown;
  };
};

type InitialData = {
  preserved?: unknown;
  updated?: unknown;
};

function readGlobalProps(): GlobalProps {
  const globalProps = lynx.__globalProps as GlobalProps;
  return {
    appTheme: globalProps.appTheme,
    flags: globalProps.flags && { ...globalProps.flags },
  };
}

export default function App() {
  const initialData = useInitData() as InitialData;
  const [globalProps, setGlobalProps] = useState(readGlobalProps);
  const theme = String(globalProps.appTheme ?? '');
  const preserved = String(initialData.preserved ?? '');
  const updated = String(initialData.updated ?? '');
  const initialFlag = String(globalProps.flags?.initial ?? '');
  const replacementFlag = String(globalProps.flags?.replacement ?? '');

  useLynxGlobalEventListener('onGlobalPropsChanged', () => {
    setGlobalProps(readGlobalProps());
  });

  useEffect(() => {
    const bridge = (NativeModules as any).bridge as any;
    bridge.call(
      'metadata-update-state',
      { theme, preserved, updated, initialFlag, replacementFlag },
      () => {}
    );
  }, [initialFlag, preserved, replacementFlag, theme, updated]);

  return (
    <view style={{ flexDirection: 'column' as const }} className="container">
      <text id="metadata-update-theme">{theme}</text>
      <text id="metadata-update-preserved">{preserved}</text>
      <text id="metadata-update-updated">{updated}</text>
      <text id="metadata-update-initial-flag">{initialFlag}</text>
      <text id="metadata-update-replacement-flag">{replacementFlag}</text>
    </view>
  );
}
