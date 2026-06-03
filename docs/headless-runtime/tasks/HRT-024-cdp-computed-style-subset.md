# HRT-024 CDP Computed Style Subset

## Objective

Expose a selected, test-oriented computed style subset through CDP so interface
E2E can validate CSS effects without requiring full Chrome CSSOM parity.

## Product Requirement

Preferred protocol shape:

```text
CSS.getComputedStyleForNode
```

or a Lynx-specific subset if full CDP CSS domain parity is not practical:

```text
LynxStyle.getComputedStyleSubset
```

The method must be protocol-facing. Platform code can back it, but tests should
not call native/private APIs directly.

## First Style Subset

- layout/box: x, y, width, height
- visibility: visible, opacity
- text: color, fontSize, fontWeight
- box styling: backgroundColor, borderWidth, borderRadius
- transform: final transform/matrix or normalized translate fields when
  available

Out of first scope:

- all CSS properties from `css_generator/index.d.ts`
- selector matching rules
- style cascade debugging
- pseudo-element style

## Backend Model

```text
CDP style method
  -> backend style bridge
      -> macOS Lynxtron runtime
      -> Android/iOS explorer app
      -> non-Lynxtron host bridge
  -> normalized subset response
```

If a backend cannot expose a property, the response should mark it unavailable
instead of guessing from screenshots.

## Acceptance

- HRT-020 CSS gallery can assert at least three style subset properties through
  CDP.
- `trace.jsonl` records style method calls and unavailable properties.
- The API shape is usable by future mobile/explorer providers.
