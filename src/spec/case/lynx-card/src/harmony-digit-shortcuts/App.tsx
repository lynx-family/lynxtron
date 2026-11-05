// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { useState } from '@lynx-js/react';

// Manual check for app-level digit shortcuts (e.g. Ctrl+1 selects the first
// tab). Shortcuts are read with global-bindkeydown so they work whether or
// not an element has focus. A plain bindkeydown counter is shown alongside to
// tell the two delivery paths apart.
const TAB_COUNT = 9;
const SHIFTED: Record<string, string> = {
  ')': '0', '!': '1', '@': '2', '#': '3', '$': '4',
  '%': '5', '^': '6', '&': '7', '*': '8', '(': '9',
};

type Mods = { ctrl: boolean; shift: boolean; alt: boolean; meta: boolean };

function readMods(event: any): Mods {
  const src = event?.detail ?? event ?? {};
  const flag = (name: string) => src[name] === true || event?.[name] === true;
  return {
    ctrl: flag('ctrlKey'),
    shift: flag('shiftKey'),
    alt: flag('altKey'),
    meta: flag('metaKey'),
  };
}

function digitOf(key: string): string | null {
  if (/^[0-9]$/.test(key)) return key;
  return SHIFTED[key] ?? null;
}

function comboName(mods: Mods, digit: string): string {
  return [
    mods.ctrl && 'Ctrl',
    mods.alt && 'Alt',
    mods.shift && 'Shift',
    mods.meta && 'Meta',
    digit,
  ]
    .filter(Boolean)
    .join('+');
}

export default function App() {
  const [activeTab, setActiveTab] = useState(0);
  const [lastCombo, setLastCombo] = useState('(none yet)');
  const [lastRaw, setLastRaw] = useState('');
  const [hits, setHits] = useState<Record<string, number>>({});
  const [globalCount, setGlobalCount] = useState(0);
  const [bubbleCount, setBubbleCount] = useState(0);

  const onGlobalKeyDown = (event: any) => {
    setGlobalCount((n) => n + 1);
    const detail = event?.detail ?? {};
    const key = detail.key ?? event?.key ?? '';
    const mods = readMods(event);
    setLastRaw(
      `key="${key}" ctrl=${mods.ctrl} shift=${mods.shift} alt=${mods.alt} ` +
        `meta=${mods.meta} repeat=${detail.repeat === true}`,
    );
    const digit = digitOf(key);
    if (!digit || detail.repeat === true) return;
    const hasModifier = mods.ctrl || mods.alt || mods.shift || mods.meta;
    if (!hasModifier) return;

    const combo = comboName(mods, digit);
    console.log(`[SHORTCUT] ${combo}`);
    setLastCombo(combo);
    setHits((prev) => ({ ...prev, [combo]: (prev[combo] ?? 0) + 1 }));

    // Ctrl+1..9 (no other modifier) selects tab N.
    if (mods.ctrl && !mods.alt && !mods.shift && !mods.meta && digit !== '0') {
      setActiveTab(Number(digit) - 1);
    }
  };

  const tabs = Array.from({ length: TAB_COUNT }, (_, i) => i);
  const hitList = Object.keys(hits).sort();

  return (
    <view
      global-bindkeydown={onGlobalKeyDown}
      bindkeydown={() => setBubbleCount((n) => n + 1)}
      style={{
        width: '100%',
        height: '100%',
        backgroundColor: '#ffffff',
        padding: '16px',
        flexDirection: 'column',
      }}
    >
      <text style={{ fontSize: '20px', color: '#111111', fontWeight: 'bold' }}>
        Digit shortcut test
      </text>
      <text style={{ fontSize: '15px', color: '#333333', marginTop: '6px' }}>
        Press Ctrl+1 ... Ctrl+9: the matching tab is selected. Also try
        Alt/Shift/Ctrl+Shift + digit, number row and numpad, with and without
        the input focused (click empty space to unfocus).
      </text>

      <view style={{ display: 'flex', flexDirection: 'row', marginTop: '10px' }}>
        {tabs.map((i) => (
          <view
            key={`tab-${i}`}
            bindtap={() => setActiveTab(i)}
            style={{
              width: '52px',
              height: '34px',
              marginRight: '4px',
              justifyContent: 'center',
              alignItems: 'center',
              backgroundColor: i === activeTab ? '#1565c0' : '#e3e8ef',
            }}
          >
            <text
              style={{
                fontSize: '15px',
                color: i === activeTab ? '#ffffff' : '#333333',
              }}
            >
              {`Tab ${i + 1}`}
            </text>
          </view>
        ))}
      </view>
      <view
        style={{
          height: '56px',
          justifyContent: 'center',
          paddingLeft: '12px',
          backgroundColor: '#f3f6fa',
        }}
      >
        <text style={{ fontSize: '18px', color: '#1565c0' }}>
          {`Content of Tab ${activeTab + 1}`}
        </text>
      </view>

      <text style={{ fontSize: '24px', color: '#111111', marginTop: '10px' }}>
        {`Last shortcut: ${lastCombo}`}
      </text>
      <text style={{ fontSize: '14px', color: '#555555', marginTop: '4px' }}>
        {`Last keydown: ${lastRaw}`}
      </text>
      <text style={{ fontSize: '14px', color: '#555555', marginTop: '2px' }}>
        {`keydown received: global=${globalCount} bubble=${bubbleCount}`}
      </text>

      <input
        placeholder="optional: focus here, shortcuts must still work"
        style={{
          width: '380px',
          height: '36px',
          marginTop: '10px',
          borderWidth: '1px',
          borderColor: '#888888',
          fontSize: '16px',
        }}
      />

      <view style={{ display: 'flex', flexDirection: 'row', marginTop: '8px', alignItems: 'center' }}>
        <text style={{ fontSize: '15px', color: '#111111' }}>Shortcuts seen:</text>
        <view
          bindtap={() => {
            setHits({});
            setLastCombo('(none yet)');
          }}
          style={{
            marginLeft: '12px',
            paddingLeft: '10px',
            paddingRight: '10px',
            height: '26px',
            justifyContent: 'center',
            backgroundColor: '#e5e5e5',
          }}
        >
          <text style={{ fontSize: '14px', color: '#111111' }}>clear</text>
        </view>
      </view>
      <text style={{ fontSize: '15px', color: '#12803f', marginTop: '4px' }}>
        {hitList.length === 0
          ? '-'
          : hitList.map((c) => `${c} x${hits[c]}`).join('   ')}
      </text>
    </view>
  );
}
