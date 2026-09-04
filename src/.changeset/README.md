This folder contains your Changesets.

Create a new changeset with `yarn changeset` and follow the prompts.
After changesets are merged into `main`, the Changesets release workflow creates
or updates a Version Packages pull request. Merging that pull request publishes
the stable runtime and npm packages from the merge commit.

Alpha releases do not consume changesets. Run the `publish` workflow manually,
provide the source branch in `source_ref`, and use an alpha tag such as
`v1.2.3-alpha.0`.
