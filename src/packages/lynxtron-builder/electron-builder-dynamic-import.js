'use strict';

const fs = require('fs');
const { pathToFileURL } = require('url');
const { resolveUpstreamDependency } = require('./module-resolver.js');

function resolveImport(request) {
  if (
    request.startsWith('file:') ||
    request.startsWith('data:') ||
    request.startsWith('node:')
  ) {
    return request;
  }
  if (fs.existsSync(request)) {
    return pathToFileURL(request).href;
  }
  return pathToFileURL(
    resolveUpstreamDependency('app-builder-lib', request)
  ).href;
}

exports.dynamicImport = async function dynamicImport(request) {
  return import(resolveImport(request));
};

exports.dynamicImportMaybe = async function dynamicImportMaybe(request) {
  try {
    return require(resolveUpstreamDependency('app-builder-lib', request));
  } catch (requireError) {
    try {
      return await exports.dynamicImport(request);
    } catch (importError) {
      requireError.message = `\n1. ${requireError.message}\n2. ${importError.message}`;
      throw requireError;
    }
  }
};
