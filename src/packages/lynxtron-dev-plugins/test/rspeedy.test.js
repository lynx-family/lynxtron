// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import assert from 'node:assert/strict';
import test from 'node:test';
import { createRsbuild } from '@rsbuild/core';
import { pluginRspeedyDevReady } from '../dist/rspeedy.js';

async function printer({ selected, filename, custom, pluginFirst = false }) {
  let api;
  let upstreamCalls = 0;
  // Reproduce the released Lynx plugin's capture of unfiltered environments.
  const upstream = {
    name: 'test-rspeedy-url-printer',
    setup(pluginApi) {
      api = pluginApi;
      api.expose(Symbol.for('rspeedy.api'), {
        config: { output: { filename } },
      });
      api.modifyRsbuildConfig((config) => {
        if (
          config.server.printUrls !== undefined &&
          config.server.printUrls !== true
        ) {
          return config;
        }
        const names = Object.keys(config.environments);
        config.server.printUrls = () => {
          upstreamCalls++;
          for (const environment of names)
            api.getNormalizedConfig({ environment });
          return [{ label: 'Original', url: 'https://original.example/' }];
        };
        return config;
      });
    },
  };
  const plugins = [upstream, pluginRspeedyDevReady()];
  const rsbuild = await createRsbuild({
    environment: selected,
    rsbuildConfig: {
      server: { printUrls: custom },
      source: { entry: { main: import.meta.filename } },
      environments: {
        web: {},
        lynx: {},
      },
      dev: { assetPrefix: 'https://localhost:<port>/bundles/' },
      plugins: pluginFirst ? plugins.reverse() : plugins,
    },
  });
  await rsbuild.initConfigs();
  return {
    print: api.getNormalizedConfig().server.printUrls,
    upstreamCalls: () => upstreamCalls,
  };
}

test('desktop-only URLs exclude web regardless of plugin registration order', async () => {
  for (const pluginFirst of [false, true]) {
    const result = await printer({ selected: ['lynx'], pluginFirst });
    assert.deepEqual(result.print({ port: 5969, routes: [] }), [
      { label: 'Lynx', url: 'https://localhost:5969/bundles/main.lynx.bundle' },
    ]);
    assert.equal(result.upstreamCalls(), 0);
  }
});

test('web-only URLs preserve custom bundle names and browser preview', async () => {
  const { print } = await printer({
    selected: ['web'],
    filename: {
      bundle: ({ entryName, platform, lazyBundle }) => {
        assert.equal(lazyBundle, false);
        return `custom/${entryName}.${platform}.bundle`;
      },
    },
  });
  assert.deepEqual(print({ port: 6000, routes: [] }), [
    {
      label: 'Web',
      url: 'https://localhost:6000/bundles/custom/main.web.bundle',
    },
    {
      label: '∟ Preview',
      url: 'https://localhost:6000/__web_preview?casename=custom%2Fmain.web.bundle',
    },
  ]);
});

test('string filenames retain entry and platform substitutions', async () => {
  const { print } = await printer({
    selected: ['lynx'],
    filename: 'app/[name]-[platform].bundle',
  });
  assert.equal(
    print({ port: 5969, routes: [] })[0].url,
    'https://localhost:5969/bundles/app/main-lynx.bundle',
  );
});

test('unfiltered development retains the upstream printer', async () => {
  const result = await printer({ selected: ['web', 'lynx'] });
  assert.deepEqual(result.print({ port: 5969, routes: [] }), [
    { label: 'Original', url: 'https://original.example/' },
  ]);
  assert.equal(result.upstreamCalls(), 1);
});

test('custom URL callbacks and disabled output are untouched', async () => {
  const custom = () => [{ label: 'Custom', url: 'https://custom.example/' }];
  for (const setting of [custom, false]) {
    const { print } = await printer({ selected: ['lynx'], custom: setting });
    assert.equal(print, setting);
  }
});
