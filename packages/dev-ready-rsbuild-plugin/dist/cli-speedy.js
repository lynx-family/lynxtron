#!/usr/bin/env node
import fs from 'fs'
import path from 'path'

async function waitForFile(filePath, timeoutMs) {
  const end = Date.now() + timeoutMs
  while (Date.now() < end) {
    try {
      const s = fs.readFileSync(filePath, 'utf-8')
      const j = JSON.parse(s)
      if (j && j.ready && j.source === 'rspeedy') return
    } catch {}
    await new Promise((r) => setTimeout(r, 500))
  }
  throw new Error('timeout')
}

async function main() {
  const file = path.resolve(process.cwd(), './output/bundle/dev-ready.json')
  const timeout = 300000
  await waitForFile(file, timeout)
  process.stdout.write('dev-ready-speedy\n')
}

main().catch((e) => {
  process.stderr.write(String(e) + '\n')
  process.exit(1)
})
