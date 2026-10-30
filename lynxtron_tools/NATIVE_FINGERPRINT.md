# Local native fingerprint experiment

This is an experiment, not a CI cache implementation. No workflow, release
logic, or existing build entry is changed. Only the opt-in local runner below
writes a dedicated native cache namespace.

## Opt-in local storage follow-up

`local_native_cache.py` now implements locked local artifact storage with byte
and entry-count budgets, LRU eviction, SHA-256 verification, safe tar extraction,
and a non-overwriting restore destination. It refuses to adopt an existing
nonempty directory without its ownership marker. It does not upload anything or
touch Habitat dependency entries. Writers prepare an archive in a temporary
directory outside Habitat, limited to the same byte budget; peak temporary plus
retained archive usage is therefore at most twice that budget (excluding build
outputs and restore destinations).

The unsafe Ninja-only digest is NOT used to authorize these local cache hits.
The storage tests use `input_key`, a conservative recursive content snapshot of
explicit input roots plus tool files and environment identity. Directory
membership is hashed, covering newly appearing optional headers. The caller
still owns input completeness; this is not a universal hermetic build system.

Real Clang/Ninja mutation tests compare restored binaries with forced rebuilds
byte-for-byte and execute both. They cover 24 hit/miss cases, including eight
seeded native mutations followed by repeated hits. Removing an optional header
hits a previous entry when it restores an identical input state. Further tests
cover tools/generators, architecture/SDK identity, symlinks, corruption, missing
metadata, parallel writers, quota enforcement and unsafe archive links.

For an isolated full Lynxtron macOS checkout marked with
`.native-cache-acceptance-checkout`, the single entry is:

```sh
bash lynxtron_tools/run_local_native_acceptance.sh /absolute/isolated-checkout
```

This prepares dependencies and runs the actual arm64 `lynxtron_app` target. It
uses a deliberately broad input snapshot, verifies inputs did not change during
compilation, then caches/restores the complete app and compares its inventory.
Only this opt-in runner writes `~/.habitat_cache/native-artifacts-v1`, limited to
2 GiB and two entries. It always builds for verification; it does not yet skip
real Lynxtron compilation on a cache hit. Real-app acceptance is pending until
the runner completes; fixture success is not a substitute.

CEF webview is in scope as a separate component identity sharing the same total
storage quota. On an isolated checkout containing #258's actual CEF build entry:

```sh
bash lynxtron_tools/run_local_native_acceptance.sh /absolute/cef-checkout cef-webview
```

The CEF path calls the component build script, then validates a cache round trip
of the staged addon, Framework, helper apps and resources. The CEF build/dist
directories are excluded from source fingerprints. Its directory-layout test
uses fixture bytes and is explicitly not a real CEF compilation or page-rendering
test. Actual CEF acceptance remains pending.

Run the real CMake/Ninja experiment with:

```sh
python3 -m unittest discover -s lynxtron_tools -p test_native_fingerprint.py
```

After a successful Ninja build, inspect a target with:

```sh
python3 lynxtron_tools/native_fingerprint.py \
  --build-dir /absolute/build --target probe \
  --tool /absolute/compiler --identity '<actual SDK/environment identity>'
```

The manifest records Ninja input content hashes, commands, explicit tool hashes,
and the supplied environment identity. Generated outputs are excluded: their
producer inputs and commands, rather than object-file contents, determine the
fingerprint. All headers in the dependency log are included conservatively.

Local acceptance covers repeatability, unrelated JavaScript changes, C source,
header, compile-definition, and explicit SDK-identity changes using real builds.

## Boundaries before cache integration

- A configured graph alone does not supply all compiler-discovered headers.
  Missing or stale dependency logs are rejected. Even a valid previous log may
  miss newly selected conditional includes until the next build. This digest
  must not yet authorize skipping compilation.
- Absolute paths remain part of the identity; cross-workspace reuse is not
  implemented. Input parsing is POSIX-only; Windows quoting is not validated.
- SDK contents, compiler subprocesses, environment variables, configuration
  generators and undeclared custom-command inputs are not discovered fully.
  Explicit tool files and an environment label do not prove hermeticity.
- Directory dependencies are recorded as directory identities, not recursively
  hashed. Changes inside undeclared input directories are not covered.
- No real Lynxtron/CEF build, cold-run reuse, Habitat upload/download, concurrent
  writer handling, or trusted publishing-cache boundary is validated here.

Next decision: obtain a conservative cold-build dependency closure (for example,
pinned dependency tree identities plus owned native input trees), or perform
dependency discovery before permitting a cache hit. Do not claim that hashing
the generated build graph alone establishes that closure.

## Follow-up local findings

The real fixture now demonstrates a concrete false hit: after building code
using `__has_include("optional.h")` while that file is absent, creating the
header leaves the warm-log fingerprint unchanged. Forced recompilation discovers
it and the executable returns the new header's value. A second clean configured
build directory is rejected because it lacks compiler dependency logs.

## Bounded storage policy

Reserve a dedicated `native-artifacts-v1` namespace, never evict Habitat's
dependency downloads. Require explicit byte and entry-count limits for the entire
native namespace, shared across all platform/architecture keys, not per key.
Production limits must be chosen within the actual Habitat allocation. The
local acceptance explicitly chooses 2 GiB/two entries and reserves free space
for compilation. This does not establish a remote backend quota.

`native_cache_budget.py` is a pure admission planner, not a storage backend:

- Evict least-recently-used entries until both byte and count limits permit the
  incoming entry; reject a single oversized artifact without evicting others.
- Evict before staging incoming bytes, so atomic-write temporary files do not
  silently double the allowed storage footprint. Failed admission remains a
  cache miss and must not fail an otherwise successful build.
- Local integration accounts for archive/metadata bytes, serializes readers and
  writers with a namespace lock, and validates namespace ownership and symlinks.
  A killed process can leave external staging files; automatic recovery of those
  temporary directories is not implemented, so repeated interrupted runs still
  require an external bounded temporary-directory lifecycle.
- Restoring a globally cached namespace also needs pruning against the current
  budget. Local quotas alone do not bound remote snapshot retention: that needs
  the actual backend's retention policy.

Five budget tests cover LRU order, byte/count caps, oversized rejection, invalid
budgets, and 100 successive admissions. They do not establish filesystem or
concurrency correctness. Run all local tests with:

```sh
python3 -m unittest discover -s lynxtron_tools -p 'test_native_*.py'
```
