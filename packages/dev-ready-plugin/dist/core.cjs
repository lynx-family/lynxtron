const path = require('path')
const fs = require('fs')

function pluginRspeedyDevReady() {
  const readyLine = 'RSPEEDY_READY'
  const serverPrefix = 'RSPEEDY_DEV_SERVER:'
  return {
    name: 'dev-ready-rspeedy-plugin', 
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
          const dist = path.join(process.cwd(), './.tmp')
          const file = path.join(dist, 'dev-ready.rspeedy.json')
          
          try {
            fs.mkdirSync(path.dirname(file), { recursive: true })
            fs.writeFileSync(
              file,
              JSON.stringify({ ready: true, source: 'rspeedy', time: Date.now() })
            )
          } catch {}
        }
      })
    }
  }
}

function pluginRspackDevReady(opts = {}) {
  return {
    name: 'dev-ready-rspack-plugin',
    apply(compiler) {
      const outDir =
        compiler && compiler.options && compiler.options.output && compiler.options.output.path
      compiler.hooks.done.tap('dev-ready-rspack-plugin', () => {
        const file =
          opts.markerFile || path.join(process.cwd(), './.tmp/dev-ready.rspack.json')
        try {
          fs.mkdirSync(path.dirname(file), { recursive: true })
          fs.writeFileSync(
            file,
            JSON.stringify({ ready: true, source: 'rspack', time: Date.now() })
          )
        } catch {}
      })
    },
  }
}

module.exports = {
  pluginRspeedyDevReady,
  pluginRspackDevReady,
}
