import { defineConfig } from '@lynx-js/rspeedy';
import { pluginReactLynx } from '@lynx-js/react-rsbuild-plugin';

function pluginEnableEventHandleRefactor() {
  return {
    name: 'contenteditable-enable-event-handle-refactor',
    setup(api) {
      api.modifyBundlerChain((chain) => {
        const templateApi = api.useExposed(Symbol.for('LynxTemplatePlugin'));
        chain.plugin('contenteditable-enable-event-handle-refactor').use(
          class EnableEventHandleRefactorPlugin {
            apply(compiler) {
              compiler.hooks.thisCompilation.tap(
                'ContenteditableEnableEventHandleRefactorPlugin',
                (compilation) => {
                  const hooks =
                    templateApi?.LynxTemplatePlugin?.getLynxTemplatePluginHooks(
                      compilation
                    );
                  if (!hooks) {
                    throw new Error(
                      'LynxTemplatePlugin hooks are unavailable for contenteditable fixture'
                    );
                  }
                  hooks.beforeEncode.tapPromise(
                    'ContenteditableEnableEventHandleRefactorPlugin',
                    async (args) => {
                      args.encodeData.sourceContent.config.enableEventHandleRefactor =
                        true;
                      return args;
                    }
                  );
                }
              );
            }
          }
        );
      });
    },
  };
}

export default defineConfig({
  output: {
    filename: {
      bundle: 'headless_contenteditable_text.bundle',
    },
  },
  source: {
    entry: {
      main: './index.tsx',
    },
  },
  plugins: [pluginReactLynx(), pluginEnableEventHandleRefactor()],
});
