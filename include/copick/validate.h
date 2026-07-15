// Validation of copick data models. Mirrors copick's pydantic field_validators.
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_VALIDATE_H
#define COPICK_VALIDATE_H

#include "copick/config.h"
#include "copick/export.h"
#include "copick/types.h"

namespace copick {

/// Validate a pickable object. Throws ValidationError if:
///  - name contains invalid characters (see sanitize_name),
///  - label == 0 (reserved for background),
///  - any color channel is outside [0, 255].
COPICK_API void validate(const PickableObject& obj);

/// Validate a point: the transformation's last row must be [0, 0, 0, 1].
COPICK_API void validate(const Point& point);

/// Validate a whole config: every object individually, plus project-wide uniqueness
/// of object names and non-null labels. Throws ValidationError on the first problem.
COPICK_API void validate(const CopickConfig& config);

}  // namespace copick

#endif  // COPICK_VALIDATE_H
