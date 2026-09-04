// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { ensureDarwinFrameworkLinks } from '../scripts/framework-links.js';

const skipOnWindows = { skip: process.platform === 'win32' };

test('restores the standard CEF Framework links after npm packaging', skipOnWindows, async () => {
  const tempDir = await fs.mkdtemp(path.join(os.tmpdir(), 'cef-framework-'));
  const frameworkRoot = path.join(
    tempDir,
    'frameworks',
    'Chromium Embedded Framework.framework'
  );
  const versionRoot = path.join(frameworkRoot, 'Versions', 'A');
  const expectedLinks = new Map([
    [path.join('Versions', 'Current'), 'A'],
    [
      'Chromium Embedded Framework',
      'Versions/A/Chromium Embedded Framework',
    ],
    ['Resources', 'Versions/A/Resources'],
    ['Libraries', 'Versions/A/Libraries'],
  ]);

  try {
    await fs.mkdir(path.join(versionRoot, 'Resources'), { recursive: true });
    await fs.mkdir(path.join(versionRoot, 'Libraries'), { recursive: true });
    await fs.writeFile(
      path.join(versionRoot, 'Chromium Embedded Framework'),
      'fixture'
    );

    assert.equal(ensureDarwinFrameworkLinks(tempDir), 4);
    for (const [linkName, linkTarget] of expectedLinks) {
      assert.equal(
        await fs.readlink(path.join(frameworkRoot, linkName)),
        linkTarget
      );
    }

    assert.equal(ensureDarwinFrameworkLinks(tempDir), 0);
  } finally {
    await fs.rm(tempDir, { recursive: true, force: true });
  }
});
