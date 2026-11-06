# Linux GTK4 desktop integration (draft)

This change connects ordinary LynxWindow instances to a GTK4 host supplied by
Lynx/Clay, with Skity rendering. It adds Linux window lifecycle, menus, dialogs,
clipboard, screen queries and desktop services. A GTK3 development backend is
retained as migration material. CEF is outside the supported scope.

## Dependencies and build status

This draft is **not independently buildable from the published dependency pins**.
`src/dependencies/DEPS.lynx` is intentionally not rolled to an unreviewed or
unpublished commit. Separate dependency changes are required:

- Lynx/Clay: optional `enable_lynx_gtk`, GTK4 View/C API and surface, task/lifecycle
  handling, Linux Skity linkage, Fontconfig and associated input fixes.
  Proposed upstream: `lynx-family/lynx`, base `develop`.
- Skity: restrict ARM NEON premultiplied span blending to RGBA storage so BGRA
  software surfaces preserve channel order, with a regression test.
  Proposed upstream: `lynx-family/skity`, base `main`.

The existing public GN linkage proposals (Lynx #7284 and Skity #376) do not
contain these GTK4 or channel-order changes. Exact reviewed dependency commits
must be pinned before a clean-checkout build can be claimed.

The experimental GTK4 profile requires Linux, `is_headless=true`,
`enable_lynx_gtk=true`, `enable_skity=true`, `use_gtk=false`, GTK/GDK 4.22 or newer,
GIO, libdbusmenu-glib, epoxy and XScreenSaver development packages. Here
`is_headless` selects the engine bridge; ordinary windows remain native windows.

Historical experiments used a custom mixed host/target toolchain and a prepared
container. Those local VM scripts and caches are not a portable build recipe and
are not included in this PR. They are not referenced by the production source
list added here. Dependency PRs and a supported clean-build profile remain
prerequisites for reproducible integration validation.

## Validation boundaries

Checks run on this isolated draft: patch whitespace/conflict checks, source
formatting, TypeScript transpilation syntax diagnostics, Python dependency-file
syntax and checks that newly declared build sources exist.
No complete build or runtime suite has passed for this extracted commit.

Earlier October 12-13 experiments used ARM64, GTK 4.22.4 and Mesa llvmpipe on
X11 and nested native Wayland. They exercised rendering, window lifetimes,
Node timers/async IO and selected menus/dialogs/clipboard/display services.
These results involved additional dirty Lynx/Skity sources and do not validate
this draft's final commit. They are context, not release acceptance.

Known remaining limits include compositor-controlled background activation
(KWin denied restore without a valid input serial), shared-session X11 clipboard
ownership interference, strict whole-image pixel parity, complete IME and input
coverage, accessibility, x64/distribution packaging, hardware GPU and long-run
validation. Other-platform regression remains pending. This is a review draft,
not complete Linux support or a GA claim.
