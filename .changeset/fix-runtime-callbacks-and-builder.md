---
'@lynx-js/lynxtron': patch
'@lynx-js/lynxtron-builder': patch
'create-lynxtron': patch
---

Support pnpm installation and application packaging. Fix resource paths in the
generated template's `dev:web` command. Restore requestAnimationFrame callbacks
on macOS. Enter the owning V8 context before TSFN callbacks and finalizers,
preventing native preload Promise and callback operations from aborting.
