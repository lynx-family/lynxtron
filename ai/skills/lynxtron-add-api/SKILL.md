---
name: "lynxtron-add-api"
description: "Adds or extends a Lynxtron API in the current project root. Invoke when the user asks for API work under src or the main Lynxtron package surface."
---

# Lynxtron Add API

Use this skill for API work in the current project root only. It covers both:

- `add-new-api`
- `extend-existing-api`

It is appropriate when the request touches native bindings, browser bindings,
declarations, module registration, or package exports in `src/...`.

## Scope

Use these paths:

- native C++: `src/shell/api`
- native build file: `src/BUILD.gn`
- native binding macro: `src/shell/common/node_bindings.cc`
- browser binding: `src/lib/browser/api`
- browser module list: `src/lib/browser/api/module-list.ts`
- declaration: `src/packages/lynxtron/apis/api`
- package export: `src/packages/lynxtron/lynxtron.js`

## Required Inputs

Collect or infer:

- change type: `add-new-api` or `extend-existing-api`
- export name
- file name
- binding name
- API shape: class, singleton, module object, or function
- method signatures
- implementation mode: `plan-only`, `scaffold`, or `real`

If the request is ambiguous, ask before editing.

Use this template when needed:

```text
Change type: add-new-api | extend-existing-api
Export name: FooBar
File name: foo-bar
Binding name: lynxtron_binding_foo_bar
API shape: class | singleton | function
Method signatures:
- create(options: FooBarOptions): FooBar
- getVersion(): string
Implementation mode: scaffold | real
Reference API: app | powerMonitor | other
```

## Workflow

### 1. Classify The Request

- Use `extend-existing-api` when the user is adding methods to an existing API
- Otherwise use `add-new-api`
- Default to `real` unless the user explicitly asks for planning or scaffolding

If the user points to a single `.d.ts`, `.ts`, `.cc`, or `.h` file, treat it as
the entry point only. Update the full API chain unless the user explicitly says
the change is type-only.

### 2. Inspect Existing Patterns

Inspect the closest examples first:

- `src/lib/browser/api/app.ts`
- `src/lib/browser/api/module-list.ts`
- `src/packages/lynxtron/apis/api/app.d.ts`
- `src/packages/lynxtron/lynxtron.js`
- `src/BUILD.gn`
- `src/shell/common/node_bindings.cc`

Match nearby naming and export style instead of inventing a new pattern.

### 3. Update Native Code

For new APIs:

- add `api_<snake_case>.cc`
- add matching `api_<snake_case>.h`
- update `src/BUILD.gn`
- update `src/shell/common/node_bindings.cc` under `LYNXTRON_BROWSER_BINDINGS`

For existing APIs:

- prefer in-place edits to existing `.cc` and `.h`
- avoid touching build or binding registration unless a new native file or new
  binding is introduced

When adding a new header, include:

- copyright header
- header guard
- required includes
- `namespace lynxtron { namespace api { ... } }`
- class declaration
- `Create(v8::Isolate* isolate)`
- `kWrapperInfo`
- `GetObjectTemplateBuilder(...)`
- `GetTypeName()`
- private declarations that match the `.cc`

### 4. Update Browser Binding

Add or update the browser binding in `src/lib/browser/api`.

Typical pattern:

- `process._linkedBinding('lynxtron_binding_<name>')`
- extract the native export
- `export default ...`

If this is `extend-existing-api`, keep the existing module entry unless the
module name or loader path changes.

### 5. Update Declarations And Exports

- add or update the declaration file in `src/packages/lynxtron/apis/api`
- update `src/packages/lynxtron/lynxtron.js` if the package surface changes
- update `src/lib/browser/api/module-list.ts` only when a new module is
  introduced or the loader path changes

## Validation

Always verify:

1. declarations match runtime names
2. module list points to the correct browser binding
3. `src/shell/common/node_bindings.cc` contains the binding when needed
4. new native files are listed in `src/BUILD.gn`
5. header and implementation stay in sync
6. diagnostics are clean for edited files

For `extend-existing-api`, also verify that no unnecessary new files or module
entries were added.

## Common Pitfalls

- editing outside `src/...`
- forgetting `src/BUILD.gn`
- forgetting `src/shell/common/node_bindings.cc`
- forgetting `src/packages/lynxtron/lynxtron.js`
- adding only `.cc` without `.h`
- changing only a declaration file when runtime changes are required

## Response Format

Use this handoff structure:

- `Change Type`: add-new-api or extend-existing-api
- `Flow`: src
- `Mode`: scaffold or implementation
- `Files`: created and updated files
- `Assumptions`: only if needed
- `Validation`: diagnostics, tests, build, or manual checks
- `Risks`: only remaining gaps

## Example Prompts

- `Add a Lynxtron API named FooBar`
- `Extend the existing API and add flush()`
- `Add a method in src/packages/lynxtron/apis/api/foo-bar.d.ts and complete the related implementation`
