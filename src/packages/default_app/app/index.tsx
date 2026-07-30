// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { root } from '@lynx-js/react';
import './index.css';

// Inlined so the bundle stays self-contained inside default_app.asar.
import logo from './assets/logo.png?inline';
import lightning from './assets/lightning.png';
import box from './assets/box.png';
import panel from './assets/panel.png';

declare const SystemInfo: {
  lynxSdkVersion?: string;
  runtimeType?: string;
};

type DefaultAppGlobalProps = {
  versions?: {
    lynxtron?: string;
    node?: string;
  };
  execPath?: string;
};

const globalProps: DefaultAppGlobalProps =
  ((lynx as any).__globalProps as DefaultAppGlobalProps) ?? {};

const JS_ENGINE_NAMES: Record<string, string> = {
  quickjs: 'PrimJS',
  v8: 'V8',
  jsc: 'JavaScriptCore',
  jsvm: 'JSVM',
};

function getVersionItems(): string[] {
  const sys = typeof SystemInfo === 'undefined' ? {} : SystemInfo;
  const items: string[] = [];
  if (globalProps.versions?.lynxtron) {
    items.push(`Lynxtron v${globalProps.versions.lynxtron}`);
  }
  if (sys.lynxSdkVersion) {
    items.push(`Lynx v${sys.lynxSdkVersion}`);
  }
  if (globalProps.versions?.node) {
    items.push(`Node v${globalProps.versions.node}`);
  }
  if (sys.runtimeType) {
    items.push(JS_ENGINE_NAMES[sys.runtimeType] ?? sys.runtimeType);
  }
  return items;
}

const LINKS = [
  {
    label: 'Docs',
    url: 'https://lynxjs.org/next/lynxtron',
    icon: panel,
  },
  {
    label: 'Repository',
    url: 'https://github.com/lynx-family/lynxtron',
    icon: box,
  },
  {
    label: 'Examples',
    url: 'https://github.com/lynx-community/lynxtron-examples',
    icon: lightning,
  },
];

function openExternal(url: string) {
  const bridge = (NativeModules as any).bridge;
  bridge?.call('open-external', { url }, () => {});
}

export default function DefaultApp() {
  const versionItems = getVersionItems();
  const execPath = globalProps.execPath ?? 'lynxtron';

  return (
    <view clip-radius="true" className="outlineFrame">
      <view className="versionBar">
        {versionItems.flatMap((item, index) => {
          const cell = (
            <text key={item} className="versionItem">
              {item}
            </text>
          );
          if (index === 0) {
            return [cell];
          }
          return [
            <text key={`${item}-separator`} className="versionSeparator">
              |
            </text>,
            cell,
          ];
        })}
      </view>
      <view className="mainArea">
        <image src={logo} mode="aspectFit" className="logo" />
        <view className="runSection">
          <text className="runHint">
            To run a local app, execute the following on the command line:
          </text>
          <view className="commandBox">
            <text className="commandText">
              <text className="commandPrompt">$ </text>
              {execPath} path-to-app
            </text>
          </view>
        </view>
      </view>
      <view className="linksRow">
        {LINKS.map((link) => (
          <view
            key={link.label}
            className="linkItem"
            bindtap={() => openExternal(link.url)}
          >
            <view className="linkCircle">
              <image src={link.icon} mode="aspectFit" className="linkIcon" />
            </view>
            <text className="linkLabel">{link.label}</text>
          </view>
        ))}
      </view>
    </view>
  );
}

root.render(<DefaultApp />);
