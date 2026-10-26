---
"@lynx-js/lynxtron-builder": patch
---

Bundle pre-patched electron-builder packages into the published
lynxtron-builder tarball. Patches now run only during publishing, so installs
do not modify shared node_modules or the pnpm store.
