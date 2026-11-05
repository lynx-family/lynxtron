// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { useState } from '@lynx-js/react';

// Shows the ctrlKey/shiftKey/altKey/metaKey flags carried by key and mouse
// events. Focus the input, then press e.g. Ctrl+A or Shift+click the box.
function describe(event: any): string {
  const src = event?.detail ?? event ?? {};
  const mods = ['ctrlKey', 'shiftKey', 'altKey', 'metaKey']
    .filter((name) => src[name] === true || event?.[name] === true)
    .map((name) => name.replace('Key', ''));
  const key = src.key ?? event?.key ?? '';
  return `${event?.type ?? '?'} key=${key} mods=[${mods.join('+')}]`;
}

export default function App() {
  const [log, setLog] = useState<string[]>([]);
  const push = (event: any) => {
    const line = describe(event);
    console.log(`[MODTEST] ${line} raw=${JSON.stringify(event?.detail ?? {})}`);
    setLog((prev) => [line, ...prev].slice(0, 14));
  };

  return (
    <view
      id="mod-probe-root"
      bindkeydown={push}
      bindkeyup={push}
      style={{
        width: '100%',
        height: '100%',
        backgroundColor: '#ffffff',
        padding: '16px',
        flexDirection: 'column',
      }}
    >
      <text style={{ fontSize: '18px', color: '#111111' }}>
        Modifier probe: focus the input and press Ctrl/Shift/Alt/Meta + key,
        or hold a modifier and click the green box.
      </text>
      <input
        placeholder="focus me, then press keys"
        style={{
          width: '320px',
          height: '40px',
          marginTop: '12px',
          borderWidth: '1px',
          borderColor: '#888888',
          fontSize: '18px',
        }}
      />
      <view
        id="mod-probe-box"
        bindmousedown={push}
        style={{
          width: '240px',
          height: '60px',
          marginTop: '12px',
          backgroundColor: '#12a86b',
          justifyContent: 'center',
          alignItems: 'center',
        }}
      >
        <text style={{ fontSize: '18px', color: '#ffffff' }}>
          modifier + click
        </text>
      </view>
      {log.map((line, index) => (
        <text
          key={`${index}-${line}`}
          style={{ fontSize: '16px', color: '#111111', marginTop: '4px' }}
        >
          {line}
        </text>
      ))}
    </view>
  );
}
