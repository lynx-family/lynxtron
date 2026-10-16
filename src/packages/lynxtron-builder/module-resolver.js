'use strict';

// Redirect only the packages that Lynxtron patches, and only for callers that are
// part of electron-builder itself. All other dependencies (including user hooks)
// use the package manager's normal resolution and stay shareable in its cache.
const Module = require('module');
const path = require('path');
const packageJson = require('./package.json');
const { verifyBundle, getVendoredPackageDir } = require('./patch.js');

// Only verify here. Generating vendor/ is done once by cli.js / prepack / pretest;
// doing it during resolver load would race the concurrent processes that the test
// runner and electron-builder spawn, each deleting and rebuilding the same vendor/.
verifyBundle();

const originalResolveFilename = Module._resolveFilename;
const packageNames = Object.keys(packageJson.lynxtronPatchedDependencies);
const electronBuilderPackageJson = require.resolve(
  'electron-builder/package.json'
);
const electronBuilderDir = path.dirname(electronBuilderPackageJson);
const electronBuilderRequire = Module.createRequire(electronBuilderPackageJson);
const upstreamRequires = Object.fromEntries(
  packageNames.map((name) => {
    const packageJsonPath = electronBuilderRequire.resolve(`${name}/package.json`);
    return [name, Module.createRequire(packageJsonPath)];
  })
);
const dynamicImportHelper = path.join(
  __dirname,
  'electron-builder-dynamic-import.js'
);
const vendoredDynamicImportHelper = path.join(
  getVendoredPackageDir('app-builder-lib'),
  'helpers',
  'dynamic-import.js'
);
// electron-builder loads this Windows target lazily and it requires app-builder-lib
// internally; treat it as an electron-builder caller so it shares the one patched
// copy instead of loading a second, unpatched one.
const squirrelWindowsSegment = `${path.sep}electron-builder-squirrel-windows${path.sep}`;

function isInside(root, filename) {
  if (!filename) {
    return false;
  }
  const relative = path.relative(root, filename);
  return !relative.startsWith('..') && !path.isAbsolute(relative);
}

function getVendorOwner(filename) {
  for (const name of packageNames) {
    if (isInside(getVendoredPackageDir(name), filename)) {
      return name;
    }
  }
  return null;
}

// Callers that must share the patched packages: electron-builder's own code and
// the Windows target it loads. User hooks and plugins are intentionally excluded
// so their own installed app-builder-lib/dmg-builder are never hijacked.
function isElectronBuilderCaller(filename) {
  return (
    isInside(electronBuilderDir, filename) ||
    (filename != null && filename.includes(squirrelWindowsSegment))
  );
}

function resolveUpstreamDependency(packageName, request) {
  return upstreamRequires[packageName].resolve(request);
}

Module._resolveFilename = function resolveLynxtronPackage(
  request,
  parent,
  isMain,
  options
) {
  const vendorOwner = getVendorOwner(parent?.filename);
  const usePatchedPackages =
    vendorOwner != null || isElectronBuilderCaller(parent?.filename);

  if (usePatchedPackages) {
    for (const name of packageNames) {
      if (request === name) {
        return originalResolveFilename.call(
          this,
          getVendoredPackageDir(name),
          parent,
          isMain,
          options
        );
      }
      if (request.startsWith(`${name}/`)) {
        return originalResolveFilename.call(
          this,
          path.join(getVendoredPackageDir(name), request.slice(name.length + 1)),
          parent,
          isMain,
          options
        );
      }
    }
  }

  if (vendorOwner != null) {
    if (request.startsWith('.') || path.isAbsolute(request)) {
      const resolved = originalResolveFilename.call(
        this,
        request,
        parent,
        isMain,
        options
      );
      return resolved === vendoredDynamicImportHelper
        ? dynamicImportHelper
        : resolved;
    }
    return resolveUpstreamDependency(vendorOwner, request);
  }

  return originalResolveFilename.call(this, request, parent, isMain, options);
};

module.exports = {
  resolveUpstreamDependency,
};
