// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { LynxWindow } from 'lynxtron';
import { expect } from 'chai';
import { closeWindow } from './lib/window-helpers';

describe('LynxWindow.setProgressBar()', () => {
  let w: LynxWindow;

  beforeEach(() => {
    w = new LynxWindow({ show: false });
  });

  afterEach(async () => {
    await closeWindow(w);
    w = (null as unknown) as LynxWindow;
  });

  it('sets the progress bar', () => {
    expect(() => {
      w.setProgressBar(0.5);
    }).to.not.throw();
  });

  it('sets the progress bar with normal mode', () => {
    expect(() => {
      w.setProgressBar(0.5, { mode: 'normal' });
    }).to.not.throw();
  });

  it('sets the progress bar with error mode', () => {
    expect(() => {
      w.setProgressBar(0.5, { mode: 'error' });
    }).to.not.throw();
  });

  it('sets the progress bar with indeterminate mode', () => {
    expect(() => {
      w.setProgressBar(0.5, { mode: 'indeterminate' });
    }).to.not.throw();
  });

  it('sets the progress bar with paused mode', () => {
    expect(() => {
      w.setProgressBar(0.5, { mode: 'paused' });
    }).to.not.throw();
  });

  it('sets the progress bar with none mode', () => {
    expect(() => {
      w.setProgressBar(0.5, { mode: 'none' });
    }).to.not.throw();
  });

  it('sets the progress bar with invalid mode', () => {
    expect(() => {
      w.setProgressBar(0.5, { mode: 'invalid' as any });
    }).to.not.throw();
  });

  it('sets the progress bar with negative value', () => {
    expect(() => {
      w.setProgressBar(-0.5);
    }).to.not.throw();
  });

  it('sets the progress bar with value > 1', () => {
    expect(() => {
      w.setProgressBar(1.5);
    }).to.not.throw();
  });
});
