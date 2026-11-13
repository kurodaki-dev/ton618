# The TON618 package registry

Every published package version is one file here: `packages/<name>-<version>.tonpkg`,
made by `ton618 build`. The website's build turns this folder into
`https://c-ton618.duckdns.org/registry/` (an `index.json` and the packages), which is what
`ton618 install` reads.

- **Publish a package:** `ton618 publish` (see the documentation chapter "Packages").
  It opens a pull request that adds one file to `packages/`.
- **`sources/`** holds the source projects of the packages maintained here, so they can be rebuilt.
- A pull request is checked by `scripts/registry.py validate`: valid format, no unsafe file
  names, dependencies that exist, every `.ton` file parses, and **published versions never change**.
