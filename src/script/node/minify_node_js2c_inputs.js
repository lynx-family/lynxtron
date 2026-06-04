#!/usr/bin/env node
// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

const fs = require('fs');
const path = require('path');
const swc = require('@swc/core');

const WRAPPER_PREFIX = 'function __node_builtin_minify__(){\n';
const WRAPPER_SUFFIX = '\n}';

function parseArgs(argv) {
  const result = {
    files: [],
  };

  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--input-dir') {
      result.inputDir = argv[++i];
    } else if (arg === '--output-dir') {
      result.outputDir = argv[++i];
    } else {
      result.files.push(arg);
    }
  }

  if (!result.inputDir || !result.outputDir || result.files.length === 0) {
    throw new Error(
      'Usage: minify_node_js2c_inputs.js --input-dir <dir> --output-dir <dir> <files...>'
    );
  }
  return result;
}

async function writeIfChanged(filename, contents) {
  let oldContents = null;
  try {
    oldContents = await fs.promises.readFile(filename, 'utf8');
  } catch (error) {
    if (error.code !== 'ENOENT') {
      throw error;
    }
  }

  if (oldContents === contents) {
    return;
  }

  await fs.promises.mkdir(path.dirname(filename), { recursive: true });
  await fs.promises.writeFile(filename, contents, 'utf8');
}

function unwrapFunctionBody(code) {
  const bodyStart = code.indexOf('{');
  const bodyEnd = code.lastIndexOf('}');
  if (bodyStart < 0 || bodyEnd <= bodyStart) {
    throw new Error('SWC did not emit the expected wrapper function');
  }
  return code.slice(bodyStart + 1, bodyEnd);
}

async function minifyFile(inputDir, outputDir, file) {
  const input = path.join(inputDir, file);
  const output = path.join(outputDir, file);
  const source = await fs.promises.readFile(input, 'utf8');
  const isModule = file.endsWith('.mjs');
  const shouldWrap = !isModule;
  const minifierInput = shouldWrap
    ? `${WRAPPER_PREFIX}${source}${WRAPPER_SUFFIX}`
    : source;
  const minified = await swc.minify(minifierInput, {
    compress: {
      keep_classnames: true,
      keep_fargs: true,
      keep_fnames: true,
      passes: 2,
    },
    ecma: 2024,
    mangle: false,
    module: isModule,
    format: {
      asciiOnly: false,
      comments: false,
      semicolons: true,
    },
    sourceMap: false,
  });

  await writeIfChanged(
    output,
    shouldWrap ? unwrapFunctionBody(minified.code) : minified.code
  );
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  for (const file of args.files) {
    await minifyFile(args.inputDir, args.outputDir, file);
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
