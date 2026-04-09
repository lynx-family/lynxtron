// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { root } from '@lynx-js/react';
import './index.css';
import FeatureGrid from './components/FeatureGrid';

export default function WebContainer() {
  return (
    <view clip-radius="true" className="outlineFrame">
      <view className="autoWrapper">
        <view className="innerFrame">
          <view className="introduction">
            <text className="lynxtronTitle">Lynxtron</text>
            <text className="contextWrapper">
              <text className="contextLight">
                Build desktop applications with Lynx.
              </text>
            </text>
            <view className="quickStartWrapper">
              <view className="quickStartBackground" />
              <view className="maskGroup">
                <text className="quickStart">Quick Start</text>
              </view>
            </view>
          </view>
          <FeatureGrid />
        </view>
      </view>
    </view>
  );
}

root.render(<WebContainer />);
