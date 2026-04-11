// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { clipboard, nativeImage } from 'lynxtron';

import { expect } from 'chai';

import * as path from 'node:path';

describe('clipboard module', () => {
  const fixturesPath = path.join(__dirname, 'fixtures');

  afterEach(() => {
    clipboard.clear();
  });

  it('reads and writes plain text', () => {
    const text = `lynxtron-clipboard-${Date.now()}`;
    clipboard.writeText(text);

    expect(clipboard.readText()).to.equal(text);
    expect(clipboard.availableFormats()).to.include('text/plain');
  });

  it('reads and writes html', () => {
    clipboard.writeHTML('<b>clipboard html</b>');

    const html = clipboard.readHTML();
    expect(html).to.contain('<meta charset');
    expect(html).to.contain('<b>clipboard html</b>');
    expect(clipboard.availableFormats()).to.include('text/html');
  });

  it('reads and writes images', () => {
    const image = nativeImage.createFromPath(
      path.join(fixturesPath, 'assets', 'logo.png')
    );

    clipboard.writeImage(image);

    const pastedImage = clipboard.readImage();
    expect(pastedImage.isEmpty()).to.be.false();
    expect(pastedImage.getSize()).to.deep.equal(image.getSize());
  });

  it('writes multiple formats at once', () => {
    clipboard.write({
      text: 'plain fallback',
      html: '<i>rich text</i>',
    });

    expect(clipboard.readText()).to.equal('plain fallback');
    expect(clipboard.readHTML()).to.contain('<i>rich text</i>');
    expect(clipboard.availableFormats()).to.include.members([
      'text/plain',
      'text/html',
    ]);
  });

  it('clears clipboard contents', () => {
    clipboard.writeText('to-be-cleared');
    clipboard.clear();

    expect(clipboard.readText()).to.equal('');
    expect(clipboard.readHTML()).to.equal('');
    expect(clipboard.readImage().isEmpty()).to.be.true();
    expect(clipboard.availableFormats()).to.not.include('text/plain');
  });
});
