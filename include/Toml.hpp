#pragma once
#include <string>
#include "Value.hpp"

// ============================================================================
// Toml.hpp — reads the subset of TOML that a tonproject.toml needs, into a
// TON618 Value (a ton.dict of ton.dicts, ton.arrays, strings, numbers...).
//
// Supported: [tables] and [dotted.tables], key = value, dotted keys, "basic"
// and 'literal' strings (with their escapes), """multi-line""" strings,
// integers (also 0x/0o/0b), floats, booleans, [arrays] (over several lines
// too), {inline = "tables"} and # comments.
// Not supported (a clear error is raised): [[arrays of tables]] and dates.
//
// `what` is only used in error messages ("tonproject.toml line 3: ...").
// ============================================================================

Value parseToml(const std::string& text, const std::string& what = "tonproject.toml");
