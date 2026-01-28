import path from 'path'
import fs from 'fs'

export function pluginDevReady(opts = {}) {
  const marker = opts.markerFile
  const readyLine = opts.readyLine ?? 'RSBUILD_READY'
  const serverPrefix = opts.serverLinePrefix ?? 'RSBUILD_DEV_SERVER:'
  return {
    name: 'dev-ready-rsbuild-plugin',
    apply: 'serve',
    setup(api) {
      let done = false
      api.onAfterStartDevServer((params) => {
        const port = params && params.port
        if (port) process.stdout.write(`${serverPrefix}${port}\n`)
      })
      api.onAfterDevCompile((params) => {
        const first = params && params.isFirstCompile
        if (!done && first) {
          done = true
          process.stdout.write(`${readyLine}\n`)
          const envs = params && params.environments
          let file = marker
          if (!file) {
            const names = envs ? Object.keys(envs) : []
            const firstEnv = names[0]
            const dist = firstEnv ? envs[firstEnv] && envs[firstEnv].distPath : process.cwd()
            file = path.join(dist, 'dev-ready.json')
          }
          try {
            fs.mkdirSync(path.dirname(file), { recursive: true })
            fs.writeFileSync(file, JSON.stringify({ ready: true, source: 'rspeedy', time: Date.now() }))
          } catch {}
        }
      })
    },
  }
}
