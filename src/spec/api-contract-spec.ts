// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { strict as assert } from 'node:assert';
import * as path from 'node:path';
import * as ts from 'typescript';

// Compare public declarations with the ESM entry in a real Lynxtron process.
// Check module members, static members and instance prototypes; ignore extras.
// APIs, getters and constructors are not invoked. Parameter types, return values,
// event behavior and fields assigned only during construction are not verified.
describe('Runtime API contract', () => {
  it('implements the API declared by the public package', async function () {
    this.timeout(30000);

    const packagePath = require.resolve('@lynx-js/lynxtron/package.json');
    const packageRoot = path.dirname(packagePath);
    const entry = path.resolve(
      packageRoot,
      require(packagePath).exports['.'].types
    );
    const program = ts.createProgram([entry], {
      noEmit: true,
      strict: true,
      module: ts.ModuleKind.CommonJS,
      target: ts.ScriptTarget.ES2020,
      types: ['node'],
      typeRoots: [path.resolve(__dirname, '../node_modules/@types')],
    });
    const diagnostics = ts.getPreEmitDiagnostics(program);
    assert.equal(
      diagnostics.length,
      0,
      ts.formatDiagnosticsWithColorAndContext(diagnostics, {
        getCanonicalFileName: (file) => file,
        getCurrentDirectory: () => process.cwd(),
        getNewLine: () => '\n',
      })
    );
    const checker = program.getTypeChecker();
    const source = program.getSourceFile(entry)!;
    const exports = checker.getExportsOfModule(
      checker.getSymbolAtLocation(source)!
    );

    // Preserve native import: ts-node loads specs as CommonJS, but this test
    // must load the package's ESM entry.
    const importModule = new Function(
      'specifier',
      'return import(specifier);'
    ) as (specifier: string) => Promise<Record<string, unknown>>;
    const runtime = await importModule('@lynx-js/lynxtron');
    const differences: string[] = [];
    const unverified = new Set<string>();
    let checked = 0;

    function onPlatform(symbol: ts.Symbol): boolean {
      return (symbol.declarations || []).some((declaration) => {
        const tags = ts
          .getJSDocTags(declaration)
          .filter((tag) => tag.tagName.text === 'platform');
        return (
          tags.length === 0 ||
          tags.some((tag) => {
            const platforms = ts
              .getTextOfJSDocComment(tag.comment)!
              .split(/[,\s]+/);
            return (
              platforms.includes(process.platform) ||
              (platforms.includes('mas') &&
                Boolean((process as NodeJS.Process & { mas?: boolean }).mas))
            );
          })
        );
      });
    }

    function descriptor(
      object: object,
      name: string
    ): PropertyDescriptor | undefined {
      for (
        let current = object;
        current;
        current = Object.getPrototypeOf(current)
      ) {
        const property = Object.getOwnPropertyDescriptor(current, name);
        if (property) return property;
      }
      return undefined;
    }

    function compare(
      type: ts.Type,
      value: unknown,
      name: string,
      ancestors: Set<ts.Type>
    ): void {
      const expected = checker.getNonNullableType(type);
      const callable =
        expected.getCallSignatures().length > 0 ||
        expected.getConstructSignatures().length > 0;
      checked++;
      if (value === undefined || value === null) {
        if (
          !checker.isTypeAssignableTo(
            value === null ? checker.getNullType() : checker.getUndefinedType(),
            type
          )
        ) {
          differences.push(`${name}: got ${String(value)}`);
        }
        return;
      }
      if (callable && typeof value !== 'function') {
        differences.push(`${name}: expected function, got ${typeof value}`);
        return;
      }
      if (ancestors.has(expected)) return;
      const next = new Set(ancestors).add(expected);
      const declaration = expected.getSymbol()?.valueDeclaration;
      if (
        expected.getConstructSignatures().length &&
        declaration &&
        ts.isClassDeclaration(declaration)
      ) {
        const prototype = descriptor(value as object, 'prototype');
        if (!prototype?.value) {
          differences.push(`${name}.prototype: missing`);
        } else {
          members(
            checker.getDeclaredTypeOfSymbol(expected.getSymbol()!),
            prototype.value,
            `${name}.prototype`,
            next,
            true
          );
        }
      }
      // Do not traverse parameter types, return types or Node/DOM built-ins.
      const localMembers = checker
        .getPropertiesOfType(expected)
        .filter((member) =>
          member.declarations?.some((node) =>
            path
              .resolve(node.getSourceFile().fileName)
              .startsWith(packageRoot + path.sep)
          )
        );
      if (localMembers.length) {
        if (typeof value !== 'object' && typeof value !== 'function') {
          differences.push(`${name}: expected object, got ${typeof value}`);
        } else {
          members(expected, value, name, next);
        }
      }
    }

    function members(
      type: ts.Type,
      value: object,
      name: string,
      ancestors: Set<ts.Type>,
      prototypeOnly = false
    ): void {
      for (const member of checker.getPropertiesOfType(type)) {
        if (member.name === 'prototype' || !onPlatform(member)) continue;
        const declaration = member.valueDeclaration || member.declarations![0];
        if (
          ts.getCombinedModifierFlags(declaration) &
          (ts.ModifierFlags.Private | ts.ModifierFlags.Protected)
        )
          continue;
        const memberPath = `${name}.${member.name}`;
        const property = descriptor(value, member.name);
        const memberType = checker.getTypeOfSymbolAtLocation(
          member,
          declaration
        );
        if (!property) {
          if (member.flags & ts.SymbolFlags.Optional) continue;
          if (prototypeOnly && !(member.flags & ts.SymbolFlags.Method)) {
            unverified.add(`${memberPath}: requires an instance`);
          } else {
            differences.push(`${memberPath}: missing`);
          }
        } else if (!('value' in property)) {
          checked++;
          unverified.add(`${memberPath}: getter/setter presence only`);
        } else {
          compare(memberType, property.value, memberPath, ancestors);
        }
      }
    }

    for (const exported of exports) {
      // Type-only exports and interfaces do not require runtime values.
      if (
        exported.declarations?.some(
          (node) =>
            ts.isExportSpecifier(node) &&
            (node.isTypeOnly || node.parent.parent.isTypeOnly)
        )
      )
        continue;
      const symbol =
        exported.flags & ts.SymbolFlags.Alias
          ? checker.getAliasedSymbol(exported)
          : exported;
      if (!(symbol.flags & ts.SymbolFlags.Value) || !onPlatform(symbol))
        continue;
      if (!Object.hasOwn(runtime, exported.name)) {
        differences.push(`${exported.name}: missing export`);
        continue;
      }
      const declaration = symbol.valueDeclaration || symbol.declarations![0];
      compare(
        checker.getTypeOfSymbolAtLocation(symbol, declaration),
        runtime[exported.name],
        exported.name,
        new Set()
      );
    }

    console.log(
      `API contract: ${checked} values checked (${process.platform})`
    );
    if (unverified.size) {
      console.log(`Limited coverage:\n${[...unverified].sort().join('\n')}`);
    }
    assert.ok(checked > 0, 'No runtime API values were checked');
    assert.equal(
      differences.length,
      0,
      `Runtime API contract mismatch:\n${differences.sort().join('\n')}`
    );
  });
});
