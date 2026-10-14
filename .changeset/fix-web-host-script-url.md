---
'@lynx-js/lynxtron': patch
---

Align Web Host declarations with the existing implementation: use `nodejs.scriptURL` for background adapter scripts and accept multiple arguments in main-thread bridge handlers. This fixes type errors in the generated Web template without changing runtime behavior.
