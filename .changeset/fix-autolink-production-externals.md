---
"@lynx-js/lynxtron-dev-plugins": patch
---

Keep AutoLink entrypoints from being externalized as ordinary production dependencies so explicit imports and automatic registration share the staged native addon. This prevents duplicate native module instances, including CEF initialization crashes, while preserving externalization for other dependencies.
