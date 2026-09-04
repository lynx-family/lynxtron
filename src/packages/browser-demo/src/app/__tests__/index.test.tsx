// Copyright 2024 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
import '@testing-library/jest-dom';
import { expect, test } from 'vitest';
import { render, getQueriesForElement } from '@lynx-js/react/testing-library';

import { App } from '../App.jsx';

test('App', async () => {
  render(<App />);
  const { findByText } = getQueriesForElement(elementTree.root!);
  expect(await findByText('Lynxtron')).toBeInTheDocument();
  expect(await findByText('Quick Access')).toBeInTheDocument();
});
