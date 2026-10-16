// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { spawn } from 'node:child_process';
import { once } from 'node:events';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';

import { expect } from 'chai';

describe('Lynx runtime shutdown', () => {
  for (const mode of ['quit', 'exit', 'closed', 'cancel', 'pending', 'empty']) {
    it(`cleans up safely on ${mode}`, async function () {
      this.timeout(60000);
      const directory = fs.mkdtempSync(
        path.join(os.tmpdir(), 'lynx-shutdown-')
      );
      const markers = path.join(directory, 'markers');
      const child = spawn(
        process.execPath,
        [path.join(__dirname, 'fixtures/api/runtime-shutdown/main.js'), mode],
        {
          env: { ...process.env, LYNXTRON_SHUTDOWN_MARKERS: markers },
        }
      );
      let output = '';
      child.stdout?.on('data', (chunk) => {
        output += chunk;
      });
      child.stderr?.on('data', (chunk) => {
        output += chunk;
      });
      const timeout = setTimeout(() => child.kill(), 45000);
      try {
        const [code, signal] = await once(child, 'exit');
        expect(signal, output).to.equal(null);
        expect(code, output).to.equal(
          mode === 'exit' || mode === 'pending' ? 23 : 0
        );
        if (mode !== 'empty' && mode !== 'pending') {
          const lines = fs.readFileSync(markers, 'utf8').trim().split('\n');
          const ids = mode === 'cancel' ? [0, 1, 2, 3] : [0, 1, 2];
          expect(
            lines.filter((line) => line.startsWith('ready:')).sort()
          ).to.deep.equal(ids.map((id) => `ready:${id}`));
          expect(
            lines.filter((line) => line.startsWith('destroyed:')).sort()
          ).to.deep.equal(ids.map((id) => `destroyed:${id}`));
        }
      } finally {
        clearTimeout(timeout);
        if (child.exitCode === null && child.signalCode === null) child.kill();
        fs.rmSync(directory, { recursive: true, force: true });
      }
    });
  }
});
