import fs from 'fs'
import path from 'path'

export function rspackDevReadyPlugin(opts = {}) {
  return {
    name: 'dev-ready-rspack-plugin',
    apply(compiler) {
      const outDir = compiler && compiler.options && compiler.options.output && compiler.options.output.path
      compiler.hooks.done.tap('dev-ready-rspack-plugin', () => {
        const file = opts.markerFile || path.join(outDir || process.cwd(), 'dev-ready.json')
        try {
          fs.mkdirSync(path.dirname(file), { recursive: true })
          fs.writeFileSync(file, JSON.stringify({ ready: true, source: 'rspack', time: Date.now() }))
        } catch {}
      })
    },
  }
}
