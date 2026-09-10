// Copyright 2024 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import '@testing-library/jest-dom';
import { expect, rs, test } from '@rstest/core';
import { render, getQueriesForElement } from '@lynx-js/react/testing-library';

import { App } from '../App.jsx';

test('App', async () => {
  const onRender = rs.fn();

  const AppRenderProbe = () => {
    onRender(`__MAIN_THREAD__: ${__MAIN_THREAD__}`);
    return <App />;
  };

  render(<AppRenderProbe />);

  expect(onRender).toHaveBeenCalledTimes(1);
  expect(onRender).toHaveBeenCalledWith('__MAIN_THREAD__: false');

  const root = elementTree.root!;
  const snapshotRoot = root.cloneNode(true) as Element;
  snapshotRoot
    .querySelector('image')
    ?.setAttribute('src', '<inline placeholder image>');
  expect(snapshotRoot).toMatchInlineSnapshot(`
    <page>
      <view
        class="Background"
      >
        <image
          class="BackgroundImage"
          src="<inline placeholder image>"
        />
        <view
          class="Container"
        >
          <text
            class="Title"
          >
            Hello, Lynxtron
          </text>
          <text
            class="Hint"
          >
            Tap card to show native dialog
          </text>
        </view>
      </view>
    </page>
  `);

  const { findByText } = getQueriesForElement(root);
  const title = await findByText('Hello, Lynxtron');
  expect(title).toBeInTheDocument();
  expect(title).toMatchInlineSnapshot(`
    <text
      class="Title"
    >
      Hello, Lynxtron
    </text>
  `);

  expect(
    await findByText('Tap card to show native dialog')
  ).toBeInTheDocument();
  expect(root.querySelector('image')?.getAttribute('src')).toMatch(
    /^data:image\/png;base64,/
  );
});
