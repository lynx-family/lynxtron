// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import fs from 'node:fs';
import path from 'node:path';

const FRAMEWORK_NAME = 'Chromium Embedded Framework.framework';
const FRAMEWORK_LINKS = [
  ['Versions/Current', 'A'],
  ['Chromium Embedded Framework', 'Versions/A/Chromium Embedded Framework'],
  ['Resources', 'Versions/A/Resources'],
  ['Libraries', 'Versions/A/Libraries'],
];

export function ensureDarwinFrameworkLinks(platformRoot) {
  const frameworkRoot = path.join(
    platformRoot,
    'frameworks',
    FRAMEWORK_NAME
  );
  let created = 0;

  for (const [linkName, linkTarget] of FRAMEWORK_LINKS) {
    const linkPath = path.join(frameworkRoot, linkName);
    const resolvedTarget = path.resolve(path.dirname(linkPath), linkTarget);

    if (!fs.existsSync(resolvedTarget)) {
      throw new Error(
        `Cannot create CEF Framework link ${linkPath}: target ${resolvedTarget} does not exist.`
      );
    }

    let existing;
    try {
      existing = fs.lstatSync(linkPath);
    } catch (error) {
      if (error?.code !== 'ENOENT') {
        throw error;
      }
    }

    if (existing !== undefined) {
      if (
        existing.isSymbolicLink() &&
        fs.readlinkSync(linkPath) === linkTarget
      ) {
        continue;
      }

      throw new Error(
        `Cannot create CEF Framework link ${linkPath}: an unexpected file already exists.`
      );
    }

    fs.mkdirSync(path.dirname(linkPath), { recursive: true });
    fs.symlinkSync(linkTarget, linkPath);
    created += 1;
  }

  return created;
}
