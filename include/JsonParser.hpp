#pragma once
#include <string>
#include "Value.hpp"

// ============================================================================
// JsonParser.hpp — parses a JSON text into a TON618 Value (see Value.hpp),
// the read side of the JSON support the interpreter already has on the write
// side via Value::toJson() (used by the always-available ton.json() native).
//
// Mapping: JSON object -> ton.dict, JSON array -> ton.array, JSON string ->
// ton.string, JSON number -> ton.int/float, JSON true/false -> ton.bool,
// JSON null -> nil. Throws std::runtime_error with a position-aware message
// on malformed input.
//
// Exposed to scripts as `json_parse(text)` once `IMPORT://ton.json` is used
// (see Interpreter::registerBuiltinJson in src/Interpreter.cpp).
// ============================================================================

Value parseJson(const std::string& text);
