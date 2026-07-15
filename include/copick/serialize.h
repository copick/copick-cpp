// JSON (de)serialization of copick data models, matching copick's on-disk format.
//
// PUBLIC HEADER — must compile under -std=c++11. The reflect-cpp machinery lives in
// src/; these functions take/return plain strings and structs.
#ifndef COPICK_SERIALIZE_H
#define COPICK_SERIALIZE_H

#include <string>

#include "copick/config.h"
#include "copick/export.h"
#include "copick/types.h"

namespace copick {

/// Parse a copick config JSON document and validate it. Throws ValidationError on
/// malformed JSON or invalid content.
COPICK_API CopickConfig config_from_json(const std::string& json);

/// Serialize a config to JSON (pretty-printed with 4-space indent by default),
/// matching copick's `json.dump(..., indent=4)`.
COPICK_API std::string config_to_json(const CopickConfig& config, bool pretty = true);

/// Parse a copick picks file (`{user}_{session}_{object}.json`). Throws on malformed JSON.
COPICK_API CopickPicksFile picks_from_json(const std::string& json);

/// Serialize a picks file to JSON (matching copick's on-disk format, incl. the
/// `transformation_` key).
COPICK_API std::string picks_to_json(const CopickPicksFile& file, bool pretty = true);

}  // namespace copick

#endif  // COPICK_SERIALIZE_H
