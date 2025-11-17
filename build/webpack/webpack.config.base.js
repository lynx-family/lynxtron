const TerserPlugin = require('terser-webpack-plugin');
const webpack = require('webpack');
const WrapperPlugin = require('wrapper-webpack-plugin');

const fs = require('node:fs');
const path = require('node:path');

const lynxtronRoot = path.resolve(__dirname, '../..');

class AccessDependenciesPlugin {
  apply (compiler) {
    compiler.hooks.compilation.tap('AccessDependenciesPlugin', compilation => {
      compilation.hooks.finishModules.tap('AccessDependenciesPlugin', modules => {
        const filePaths = modules.map(m => m.resource).filter(p => p).map(p => path.relative(lynxtronRoot, p));
        console.info(JSON.stringify(filePaths));
      });
    });
  }
}

module.exports = ({
  loadElectronFromAlternateTarget,
  target
}) => {
  let entry = path.resolve(lynxtronRoot, 'lib', target, 'init.ts');
  if (!fs.existsSync(entry)) {
    entry = path.resolve(lynxtronRoot, 'lib', target, 'init.js');
  }

  const lynxtronAPIFile = path.resolve(lynxtronRoot, 'lib', loadElectronFromAlternateTarget || target, 'api', 'exports', 'lynxtron.ts');

  return (env = {}, argv = {}) => {
    const onlyPrintingGraph = !!env.PRINT_WEBPACK_GRAPH;
    const outputFilename = argv['output-filename'] || `${target}.bundle.js`;

    const defines = {
      BUILDFLAG: onlyPrintingGraph ? '(a => a)' : ''
    };

    if (env.buildflags) {
      const flagFile = fs.readFileSync(env.buildflags, 'utf8');
      for (const line of flagFile.split(/(\r\n|\r|\n)/g)) {
        const flagMatch = line.match(/#define BUILDFLAG_INTERNAL_(.+?)\(\) \(([01])\)/);
        if (flagMatch) {
          const [, flagName, flagValue] = flagMatch;
          defines[flagName] = JSON.stringify(Boolean(parseInt(flagValue, 10)));
        }
      }
    }

    const ignoredModules = [];

    const plugins = [];

    if (onlyPrintingGraph) {
      plugins.push(new AccessDependenciesPlugin());
    }

    plugins.push(new webpack.ProvidePlugin({
      Promise: ['@lynxtron/internal/common/webpack-globals-provider', 'Promise']
    }));

    plugins.push(new webpack.DefinePlugin(defines));

    return {
      mode: 'development',
      devtool: false,
      entry,
      target: 'node',
      output: {
        filename: outputFilename
      },
      resolve: {
        alias: {
          '@lynxtron/internal': path.resolve(lynxtronRoot, 'lib'),
          lynxtron$: lynxtronAPIFile,
          'lynxtron/main$': lynxtronAPIFile,
          // Force timers to resolve to our dependency that doesn't use window.postMessage
          timers: path.resolve(lynxtronRoot, 'node_modules', 'timers-browserify', 'main.js')
        },
        extensions: ['.ts', '.js'],
        fallback: {
          // We provide our own "timers" import above, any usage of setImmediate inside
          // one of our renderer bundles should import it from the 'timers' package
          setImmediate: false
        }
      },
      module: {
        rules: [{
          test: (moduleName) => !onlyPrintingGraph && ignoredModules.includes(moduleName),
          loader: 'null-loader'
        }, {
          test: /\.ts$/,
          loader: 'ts-loader',
          options: {
            configFile: path.resolve(lynxtronRoot, 'tsconfig.lynxtron.json'),
            transpileOnly: onlyPrintingGraph,
            ignoreDiagnostics: [
              // File '{0}' is not under 'rootDir' '{1}'.
              6059,
              // Private field '{0}' must be declared in an enclosing class.
              1111
            ]
          }
        }]
      },
      node: {
        __dirname: false,
        __filename: false
      },
      optimization: {
        minimize: env.mode === 'production',
        minimizer: [
          new TerserPlugin({
            terserOptions: {
              keep_classnames: true,
              keep_fnames: true
            }
          })
        ]
      },
      plugins
    };
  };
};
