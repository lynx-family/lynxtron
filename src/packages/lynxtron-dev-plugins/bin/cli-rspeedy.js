#!/usr/bin/env node
// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import fs from 'fs'
import path from 'path'

async function waitForFile(filePath, timeoutMs) {
  const start = Date.now()
  const end = start + timeoutMs

  while (Date.now() < end) {
    try {
      const s = fs.readFileSync(filePath, 'utf-8')
      const j = JSON.parse(s)
      if (j && j.ready)
        {
          try {
            fs.rmSync(filePath, { force: true })
          } catch {}
          return
        }
    } catch {}
    await new Promise((r) => setTimeout(r, 500))
  }
  throw new Error('timeout')
}

async function main() {
  const file = path.resolve(process.cwd(), './.tmp/dev-ready.rspeedy.json')
  const timeout = 300000
  await waitForFile(file, timeout)
  process.stdout.write('dev-ready-speedy\n')
}

main().catch((e) => {
  process.stderr.write(String(e) + '\n')
  process.exit(1)
})
