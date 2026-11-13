#pragma once
#include <functional>
#include <string>
#include <vector>

// ============================================================================
// Packages.hpp — the TON618 package manager, the part of the command line
// that works like pip / npm / cargo:
//
//   ton618 init [name]            new module project (tonproject.toml, ...)
//   ton618 build                  tonproject.toml -> dist/<name>-<version>.tonpkg
//   ton618 register / login / logout / whoami   account on the registry server
//   ton618 publish                build, then upload it to the registry
//   ton618 install [pkg[@range]]  from the registry into ./modules/
//   ton618 update [pkg]           newest versions the dependants allow
//   ton618 uninstall <pkg>        remove a package and its files
//   ton618 list [--outdated]      what is installed
//   ton618 search [text] / info <pkg>
//
// The registry is a small server with a SQLite database (server/): it serves
// /registry/index.json plus one .tonpkg file per version. See DOCUMENTATION.md > "Packages".
// ============================================================================

bool isPackageCommand(const std::string& command);

// `args[0]` is the command. The two callbacks are the older installer and
// uninstaller (modules registered in the original registry, which only knows
// a GitHub link): used when a name isn't in the new registry.
int runPackageCommand(const std::vector<std::string>& args,
                      const std::function<int(const std::string&)>& legacyInstall,
                      const std::function<int(const std::string&)>& legacyUninstall);

// Called by the original installer after it put modules/<name>.ton in place, so
// that `ton618 update` knows where the module came from.
void recordLegacyInstall(const std::string& name, const std::string& version, const std::string& github);
