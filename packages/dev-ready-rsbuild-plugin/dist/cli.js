#!/usr/bin/env node
import fs from 'fs'
import path from 'path'

function getArg(name) {
  const i = process.argv.findIndex((a) => a === name || a === name.replace(/^--/, '-'))
  if (i >= 0) return process.argv[i + 1]
  const pref = name + '='
  const v = process.argv.find((a) => a.startsWith(pref))
  return v ? v.slice(pref.length) : undefined
}

async function waitForFile(filePath, timeoutMs) {
  const end = Date.now() + timeoutMs
  while (Date.now() < end) {
    try {
      const s = fs.readFileSync(filePath, 'utf-8')
      const j = JSON.parse(s)
      if (j && j.ready) return
    } catch {}
    await new Promise((r) => setTimeout(r, 500))
  }
  throw new Error('timeout')
}

async function main() {
  const fileArg = getArg('--file') || getArg('-f') || './output/bundle/dev-ready.json'
  const timeoutArg = getArg('--timeout') || getArg('-t')
  const timeout = timeoutArg ? Number(timeoutArg) : 300000
  const resolved = path.resolve(process.cwd(), fileArg)
  await waitForFile(resolved, timeout)
  process.stdout.write('dev-ready\n')
}

main().catch((e) => {
  process.stderr.write(String(e) + '\n')
  process.exit(1)
})
