# HRT-019 Commerce Agent Comparison PRD

## Objective

Compare two agent workflows against the same Lynx page requirement:

- Agent A gets the local `lynxtron-headless` harness skill and must implement a
  verifiable true-headless fixture.
- Agent B gets the Lynx DevTool skill and the existing Lynxtron shell app
  context, then implements the same product surface through the shell workflow.

The comparison measures implementation completeness, verification strength,
time-to-first-valid-output, and how much product intent survives when the agent
does or does not receive the harness skill.

## Standard Data Mock

The source of truth is:

```text
docs/headless-runtime/experiments/HRT-019-commerce-standard-mock.ts
```

Workers may copy the data into their owned fixture/app directory if importing
from `docs/` is inconvenient, but they must preserve product IDs, text labels,
prices, ordering, history entries, and settings entries exactly. This is a
fixed page data fixture, not a runtime JSON mock DSL.

## Target Device

- Viewport: `390 x 844`
- DPR: `3`
- No network access required.
- No external image assets. Use styled product tiles, color blocks, badges, or
  layout-only placeholders.
- Text must be readable and must not overlap at the target viewport.

## Product Scope

Build a small ecommerce Lynx app with three user-facing surfaces:

- Recommend home page.
- Product detail page.
- Profile page.

The page must feel like a mobile ecommerce app, not a static demo. Use dense but
clear product cards, bottom tabs, scrollable content, and visible state changes.

## Functional Requirements

### Home: Recommend Tab

- Show a top bar with `Lynx Market`, `Daily Picks`, and `Cart 2`.
- Show category chips: `All`, `Home`, `Wear`, `Tech`.
- Show a two-column product recommendation list using all ten products from the
  standard mock.
- The list must be vertically scrollable in the app layout. At least the first
  six products should be visually represented in a two-column grid at the target
  viewport; the remaining products should be reachable by scroll or represented
  below the fold.
- Each product card must show product name, price, rating, sold count, and
  badge.
- The `Nova Desk Lamp` card must include a unique tappable text:
  `Open Nova Desk Lamp`.

### Product Detail

When the user taps `Open Nova Desk Lamp`, navigate to a detail surface for
`p-nova-lamp`.

The detail surface must show:

- `Product Detail`
- `Detail Nova Desk Lamp`
- `$48`
- `Inventory 24`
- `Delivery tomorrow`
- `Color Sage`
- `Warm dimmable light for compact workspaces.`
- A unique primary action text: `Add Nova To Cart`
- A back affordance with text: `Back To Recommendations`

The detail page should keep the ecommerce app feeling intact. It may keep or
hide the bottom tabs, but it must provide a visible way back to the recommendation
surface.

### Bottom Tabs

- A bottom tab bar must be visible on the home page.
- Tab labels must be exactly `Recommend` and `Profile`.
- The default selected tab is `Recommend`.
- Tapping `Profile` navigates to the profile page.
- Tapping `Recommend` from the profile page returns to the home page.

### Profile Page

The profile page must show:

- `Profile Center`
- `Mina Chen`
- `Gold member`
- `Points 4820`
- `Coupons 6`
- Section title `History`
- All three history rows from the standard mock:
  - `Viewed Nova Desk Lamp`
  - `Viewed Canvas Mini Tote`
  - `Bought Luma Braided Cable`
- Section title `Settings`
- All three settings rows from the standard mock:
  - `Address Book`
  - `Payment Methods`
  - `Notification Settings`

## Mandatory Smoke Scenarios

### Scenario 1: Product Navigation

Initial state must contain:

- `Lynx Market`
- `Daily Picks`
- `Open Nova Desk Lamp`
- `Recommend`
- `Profile`

Action:

- Tap `Open Nova Desk Lamp`.

After state must contain:

- `Product Detail`
- `Detail Nova Desk Lamp`
- `$48`
- `Inventory 24`
- `Add Nova To Cart`
- `Back To Recommendations`

### Scenario 2: Profile Tab

Initial state must contain:

- `Lynx Market`
- `Recommend`
- `Profile`

Action:

- Tap `Profile`.

After state must contain:

- `Profile Center`
- `Mina Chen`
- `Gold member`
- `Points 4820`
- `History`
- `Viewed Nova Desk Lamp`
- `Settings`
- `Notification Settings`

## Agent A: Harness Path

Owned directory:

```text
src/packages/lynxtron-headless/fixtures/commerce-harness-agent/
```

Required implementation:

- Create a self-contained Lynx React fixture.
- Use the standard mock data exactly.
- Build to `dist/headless_commerce_harness.bundle`.
- Provide a small assertion script that validates Scenario 1 and Scenario 2 UI
  dumps.

Required verification:

```sh
npm --prefix src/packages/lynxtron-headless/fixtures/commerce-harness-agent run build
```

```sh
node src/packages/lynxtron-headless/cli.js run \
  src/packages/lynxtron-headless/fixtures/commerce-harness-agent/dist/headless_commerce_harness.bundle \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/hrt-019-commerce-harness-detail \
  --timeout 12000 \
  --width 390 \
  --height 844 \
  --dpr 3 \
  --tap-text "Open Nova Desk Lamp"
```

```sh
node src/packages/lynxtron-headless/cli.js run \
  src/packages/lynxtron-headless/fixtures/commerce-harness-agent/dist/headless_commerce_harness.bundle \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/hrt-019-commerce-harness-profile \
  --timeout 12000 \
  --width 390 \
  --height 844 \
  --dpr 3 \
  --tap-text "Profile"
```

Acceptance requires both runs to produce `report.status: success`,
`backend: windowless-software`, `input.tap.protocol: cdp`, and
`input.tap.changed: true`.

## Agent B: DevTool And Shell Path

Owned directory:

```text
src/packages/lynxtron-shell-demo/src/app/components/commerce-devtool-agent/
```

Additional owned files are allowed only if needed to mount the component in the
shell demo app, and must be listed in the delivery note.

Required implementation:

- Implement the same app surface using the existing Lynxtron shell project.
- Use the standard mock data exactly.
- Prefer existing shell-demo patterns and avoid changing unrelated demo
  behavior.
- Verify through a headful or shell app launch plus Lynx DevTool DOM/screenshot
  where possible.

Required verification target:

- Build the shell demo if the repo supports it locally.
- Launch the shell demo with the implemented page.
- Use Lynx DevTool to confirm the initial home state.
- Use Lynx DevTool input or direct interaction to reach:
  - product detail after `Open Nova Desk Lamp`
  - profile page after `Profile`
- Capture at least one screenshot or DOM dump as evidence.

If full shell-demo mount is blocked, Agent B must stop and report the exact
blocker instead of silently creating an unmounted component.

## Comparison Rubric

Score each path from 0 to 2:

- Product completeness: all required surfaces and text states exist.
- Interaction correctness: product click and profile tab produce the expected
  UI state.
- Verification strength: evidence is executable and artifact-backed.
- Integration cleanliness: changes are scoped to owned files and local patterns.
- Agent efficiency: elapsed time and number of PM follow-up prompts required.

Higher value for this product direction comes from stable, repeatable
verification, not from the prettiest single screenshot.

## PM Review Checklist

- Both agents used the same standard mock.
- Both implemented the same visible requirements.
- Agent A produced true-headless artifacts for both smoke scenarios.
- Agent B produced shell/devtool evidence or a precise blocker.
- The comparison distinguishes product capability from implementation polish.

## Result Record

Status: complete.

| Path | Product completeness | Interaction correctness | Verification strength | Integration cleanliness | Agent efficiency | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| Agent A: harness | 1 | 2 | 2 | 2 | 2 | Passed both true-headless smoke scenarios with artifact-backed assertions. Product gap: the home list uses a regular `view` rather than `scroll-view`, so scrollability is not proven. |
| Agent B: devtool/shell | 2 | 2 | 1 | 2 | 1 | Product behavior and scroll-view structure are present, and DevTool verified home, detail, and profile states. Verification is weaker because the shell-demo `npm run build` fails in the desktop host step and PM had to assemble a temporary shell app for validation. |

Scores use the rubric above: `0` missing, `1` partial, `2` complete.

## Review Evidence

### Agent A: Harness

Files:

- `src/packages/lynxtron-headless/fixtures/commerce-harness-agent/package.json`
- `src/packages/lynxtron-headless/fixtures/commerce-harness-agent/lynx.config.mjs`
- `src/packages/lynxtron-headless/fixtures/commerce-harness-agent/index.tsx`
- `src/packages/lynxtron-headless/fixtures/commerce-harness-agent/index.css`
- `src/packages/lynxtron-headless/fixtures/commerce-harness-agent/assert-ui-dump.mjs`
- `src/packages/lynxtron-headless/fixtures/commerce-harness-agent/dist/headless_commerce_harness.bundle`

Worker verification:

- `npm --prefix src/packages/lynxtron-headless/fixtures/commerce-harness-agent run build`: passed.
- Detail run with `--tap-text "Open Nova Desk Lamp"`: passed.
- Profile run with `--tap-text "Profile"`: passed.
- Scenario 1 assertion script: passed.
- Scenario 2 assertion script: passed.

PM re-verification:

- Built `dist/headless_commerce_harness.bundle`: passed.
- Ran detail smoke at `/tmp/hrt-019-commerce-harness-detail-review`: passed.
- Ran profile smoke at `/tmp/hrt-019-commerce-harness-profile-review`: passed.
- `assert-ui-dump.mjs scenario1`: passed.
- `assert-ui-dump.mjs scenario2`: passed.

Observed report fields:

- Detail: `status=success`, `backend=windowless-software`,
  `input.tap.protocol=cdp`, `input.tap.changed=true`,
  `targetText=Open Nova Desk Lamp`, `source=ui-dump`.
- Profile: `status=success`, `backend=windowless-software`,
  `input.tap.protocol=cdp`, `input.tap.changed=true`,
  `targetText=Profile`, `source=ui-dump`.

Gap:

- The home list is represented as a regular `view`, not a `scroll-view`, so the
  product requirement for a scrollable recommendation list is not implemented
  strongly enough even though all smoke text states pass.

### Agent B: DevTool And Shell

Files:

- `src/packages/lynxtron-shell-demo/src/app/App.tsx`
- `src/packages/lynxtron-shell-demo/src/app/components/commerce-devtool-agent/index.tsx`
- `src/packages/lynxtron-shell-demo/src/app/components/commerce-devtool-agent/mock.ts`
- `src/packages/lynxtron-shell-demo/src/app/components/commerce-devtool-agent/styles.css`

Build result:

- `npm --prefix src/packages/lynxtron-shell-demo run build`: partial.
- `rspeedy build`: passed and produced
  `src/packages/lynxtron-shell-demo/output/bundle/lynx/main.lynx.bundle`.
- `rspack build`: failed with
  `TypeError [ERR_UNKNOWN_FILE_EXTENSION]: Unknown file extension ".ts" for
  .../src/packages/lynxtron-shell-demo/rspack.config.ts`.

PM verification workaround:

- Copied the existing shell-demo desktop host to
  `/tmp/hrt-019-commerce-devtool-app`.
- Replaced `main.lynx.bundle` with the newly built rspeedy output.
- Launched
  `out/Release/lynxtron.app/Contents/MacOS/lynxtron /tmp/hrt-019-commerce-devtool-app`.
- DevTool client: `localhost:8901`.
- DevTool session: `1`,
  `file:///private/tmp/hrt-019-commerce-devtool-app/main.lynx.bundle`.

DevTool evidence:

- Initial DOM contained `Lynx Market`, `Daily Picks`, `Open Nova Desk Lamp`,
  `Recommend`, `Profile`, and all ten product entries.
- Tapping the box for `Open Nova Desk Lamp` changed DOM to product detail with
  `Product Detail`, `Detail Nova Desk Lamp`, `$48`, `Inventory 24`,
  `Delivery tomorrow`, `Color Sage`, `Add Nova To Cart`, and
  `Back To Recommendations`.
- Tapping `Back To Recommendations`, then `Profile`, changed DOM to profile
  with `Profile Center`, `Mina Chen`, `Gold member`, `Points 4820`,
  `Coupons 6`, `History`, `Viewed Nova Desk Lamp`, `Settings`, and
  `Notification Settings`.
- Screenshot captured:
  `/var/folders/kw/95rr219x7yn1b29gnfxgjrbh0000gn/T/lynx-devtool-mcp-9Ovjbt/screenshot-Lynx_getScreenshot.jpeg`.

Gaps:

- Agent B did not return a final delivery note before the agent became
  unavailable, so PM had to complete verification locally.
- Verification depends on a temporary shell app because the shell-demo desktop
  host build failed in the current Node/tooling environment.
- The visual shell uses the existing desktop window size rather than a strict
  `390 x 844` device profile, although the component constrains the app body to
  a mobile-width layout.

## Comparison Conclusion

The harness path gives substantially stronger repeatability: one worker can
produce a fixture, build it, run true-headless CDP automation twice, assert UI
dumps, and return stable artifacts. It also exposes product gaps clearly because
the artifact assertions are repeatable.

The shell/devtool path is useful for visual inspection and validates that a
normal Lynxtron shell can host the same UI, but it is weaker as an agent-facing
skill surface today: build tooling can fail outside the page code, viewport
control is implicit, and verification requires PM-operated DevTool steps rather
than a compact repeatable command.

Product takeaway:

- The headless harness skill is already valuable for CI/agent smoke creation and
  acceptance review.
- The DevTool/shell path remains valuable for human-visible debugging and
  visual inspection, but needs a standardized launch/build harness before it can
  compete with the headless harness on agent efficiency.

## Fix Iteration

Status: complete.

### Agent A Fix Request

Problem:

- The harness fixture passes both smoke scenarios, but the recommendation list
  is implemented with a regular `view` instead of a scroll container.

Required fix:

- Replace the home recommendation list container with a real Lynx scroll
  surface, for example `scroll-view`, while preserving the current visual
  quality.
- Keep all ten standard mock products in the two-column layout.
- Preserve the two mandatory smoke scenarios and existing assertion script.
- Extend the assertion script or add a focused check proving the home UI tree
  contains a scroll container for the recommendation list.

Acceptance:

- `npm --prefix src/packages/lynxtron-headless/fixtures/commerce-harness-agent run build` passes.
- Detail smoke with `--tap-text "Open Nova Desk Lamp"` passes.
- Profile smoke with `--tap-text "Profile"` passes.
- Scenario 1 and Scenario 2 assertion checks pass.
- The added scroll-container check passes against a fresh home UI dump.
- No shell-demo files are modified.

### Agent B Fix Request

Problem:

- The shell/devtool implementation reaches all required UI states, but screenshots
  show major visual overflow and viewport mismatch: header text wraps, chips
  overlap, cards overflow, and profile content only occupies the left side of
  the desktop shell.

Required fix:

- Make the shell-demo commerce component visually stable in the existing
  Lynxtron shell screenshot path.
- The app must present as a centered or correctly constrained mobile surface
  without text overlap in home, detail, and profile states.
- Header, chips, product cards, detail hero/content, profile header, settings,
  and bottom tabs must all fit their containers.
- Keep the same standard mock data and interactions.
- Do not solve this by removing required text or dropping product fields.

Acceptance:

- `rspeedy build` for shell-demo app output passes. Full `npm run build` is
  desirable but can remain blocked by the existing `rspack.config.ts` Node 22
  loader issue if the blocker is recorded.
- Launching the shell app with the newly built Lynx bundle succeeds.
- DevTool DOM verifies home, detail, and profile states.
- DevTool screenshots of home, detail, and profile show no obvious text overlap
  or left-side-only squeezed layout.
- No headless fixture files are modified.

### Fix Iteration Result

| Path | Fix status | Verification | Remaining gaps | Efficiency note |
| --- | --- | --- | --- | --- |
| Agent A: harness fix | Accepted | Worker changed the home recommendation list to `scroll-view`, rebuilt `dist/headless_commerce_harness.bundle`, and added `home-scroll` assertion. PM re-ran build, detail smoke, profile smoke, Scenario 1 assertion, Scenario 2 assertion, and `home-scroll`; all passed on the second independent run. | One independent PM detail run first returned `INPUT_NO_VISUAL_CHANGE`, then a clean rerun passed. Treat as a low-priority stability observation for tap/frame-change detection rather than a product failure. | Best result. Agent returned a complete delivery note and artifact-backed evidence by `2026-05-29 10:23:50`; no PM follow-up was needed to complete the fix. |
| Agent B: shell visual fix | Partially accepted for visual outcome; not accepted as an autonomous agent delivery | `rspeedy build` passed and produced `src/packages/lynxtron-shell-demo/output/bundle/lynx/main.lynx.bundle`; full `npm run build` still fails at `rspack.config.ts` with Node 22 `ERR_UNKNOWN_FILE_EXTENSION`. PM assembled `/tmp/hrt-019-commerce-devtool-app-fix-review`, launched Release Lynxtron, and used DevTool to verify home -> detail -> back -> profile. Screenshots saved to `/tmp/hrt-019-commerce-devtool-fix-review/home.jpeg`, `/tmp/hrt-019-commerce-devtool-fix-review/detail.jpeg`, and `/tmp/hrt-019-commerce-devtool-fix-review/profile.jpeg`. | Agent B did not return a final delivery note after the wait window, so PM had to inspect and verify the current workspace state. The visual overflow and left-side-only squeeze are materially improved, but shell verification still depends on temporary app assembly and desktop viewport behavior. | Slower and weaker. The implementation result became inspectable, but the agent did not close the loop independently; PM verification completed at `2026-05-29 10:36:32`. |

### Fix Iteration PM Evidence

Agent A PM commands:

- `npm --prefix src/packages/lynxtron-headless/fixtures/commerce-harness-agent run build`
- `node src/packages/lynxtron-headless/cli.js run ... --artifact-dir /tmp/hrt-019-commerce-harness-detail-fix-review2 --tap-text "Open Nova Desk Lamp"`
- `node src/packages/lynxtron-headless/cli.js run ... --artifact-dir /tmp/hrt-019-commerce-harness-profile-fix-review2 --tap-text "Profile"`
- `node src/packages/lynxtron-headless/cli.js run ... --artifact-dir /tmp/hrt-019-commerce-harness-home-scroll-fix-review2`
- `node src/packages/lynxtron-headless/fixtures/commerce-harness-agent/assert-ui-dump.mjs scenario1 /tmp/hrt-019-commerce-harness-detail-fix-review2`
- `node src/packages/lynxtron-headless/fixtures/commerce-harness-agent/assert-ui-dump.mjs scenario2 /tmp/hrt-019-commerce-harness-profile-fix-review2`
- `node src/packages/lynxtron-headless/fixtures/commerce-harness-agent/assert-ui-dump.mjs home-scroll /tmp/hrt-019-commerce-harness-home-scroll-fix-review2`

Agent B PM evidence:

- `npm --prefix src/packages/lynxtron-shell-demo run build`: `rspeedy build` passed; `rspack build` failed with the existing Node 22 TypeScript config loader issue.
- DevTool client: `localhost:8902`.
- DevTool session: `1`,
  `file:///private/tmp/hrt-019-commerce-devtool-app-fix-review/main.lynx.bundle`.
- Home screenshot: `/tmp/hrt-019-commerce-devtool-fix-review/home.jpeg`.
- Detail screenshot: `/tmp/hrt-019-commerce-devtool-fix-review/detail.jpeg`.
- Profile screenshot: `/tmp/hrt-019-commerce-devtool-fix-review/profile.jpeg`.

Fix iteration conclusion:

- The harness path converted a product gap into a precise, executable
  acceptance check quickly.
- The shell/devtool path can repair visual issues, but without a standardized
  build/launch/screenshot command it remains slower to delegate and harder to
  accept without PM intervention.
