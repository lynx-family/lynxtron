This folder contains your Changesets.

Install dependencies in `src` with `node tools/yarn.js install --immutable`.
From the repository root, create a changeset with `npm run changeset` and follow
the prompts. Run `npm run version-packages` to apply pending changesets locally.
The existing `yarn changeset`, `yarn version-packages`, and `yarn release` commands
inside `src` forward to these root commands.

The root `package.json` lists the same workspaces as `src/package.json`, prefixed
with `src/`, for Changesets package discovery. Keep these lists in sync; dependency
installation and the Yarn lockfile remain in `src`.
After changesets are merged into `main`, the Changesets release workflow creates
or updates a Version Packages pull request. Merging that pull request publishes
the stable runtime and npm packages from the merge commit.

Alpha releases do not consume changesets. Run the `publish` workflow manually,
provide the source branch in `source_ref`, and use an alpha tag such as
`v1.2.3-alpha.0`.
