# The TON618 package registry (seed files)

`packages/<name>-<version>.tonpkg` are the packages maintained in this repository
(`requests`, `stringutils`), made by `ton618 build` from `sources/`.

The live registry is the SQLite-backed server in [`../server/`](../server/README.md): accounts,
API keys, `ton618 publish`. Load these files into a new server with
`python3 server/registry_server.py import registry/packages --owner <you>`.

`scripts/registry.py validate` checks this folder (valid format, no unsafe file names,
dependencies that exist, every `.ton` file parses); the server applies the same rules to every
upload, and published versions never change.
