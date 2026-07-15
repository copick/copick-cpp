// Name sanitization for object names / user ids / session ids.
// (copick.util.escape.sanitize_name)
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_ESCAPE_H
#define COPICK_ESCAPE_H

#include <string>

#include "copick/export.h"

namespace copick {

/// Replace characters invalid in a copick name with '-', then trim leading/trailing
/// dashes. Invalid characters are: <>:"/\|?* , control chars, whitespace, and '_'.
///
/// If `was_modified` is non-null it is set to true when the result differs from the
/// input. Throws ValidationError if the result would be empty.
COPICK_API std::string sanitize_name(const std::string& input, bool* was_modified = nullptr);

/// True if `input` is already a valid copick name (sanitize_name leaves it unchanged
/// and non-empty).
COPICK_API bool is_valid_name(const std::string& input);

}  // namespace copick

#endif  // COPICK_ESCAPE_H
