#include "copick/validate.h"

#include <cmath>
#include <set>
#include <string>

#include "copick/errors.h"
#include "copick/escape.h"

namespace copick {

void validate(const PickableObject& obj) {
  // name: must be already-sanitized (copick rejects rather than auto-cleans here).
  if (!is_valid_name(obj.name)) {
    throw ValidationError(
        "Name '" + obj.name +
        "' contains invalid characters. Use copick::sanitize_name() to clean it.");
  }
  // label: 0 is reserved for background.
  if (obj.label == 0) {
    throw ValidationError("Label 0 is reserved for background.");
  }
  // color: each channel in [0, 255] (length 4 is guaranteed by the type).
  for (int channel : obj.color) {
    if (channel < 0 || channel > 255) {
      throw ValidationError("Color values must be in the range [0, 255].");
    }
  }
}

void validate(const Point& point) {
  const std::array<double, 4>& last = point.transformation[3];
  const bool ok = std::abs(last[0]) < 1e-9 && std::abs(last[1]) < 1e-9 &&
                  std::abs(last[2]) < 1e-9 && std::abs(last[3] - 1.0) < 1e-9;
  if (!ok) {
    throw ValidationError("Last row of transformation matrix must be [0, 0, 0, 1].");
  }
}

void validate(const CopickConfig& config) {
  if (config.user_id && !is_valid_name(*config.user_id)) {
    throw ValidationError(
        "user_id '" + *config.user_id +
        "' contains invalid characters. Use copick::sanitize_name() to clean it.");
  }
  if (config.session_id && !is_valid_name(*config.session_id)) {
    throw ValidationError(
        "session_id '" + *config.session_id +
        "' contains invalid characters. Use copick::sanitize_name() to clean it.");
  }

  std::set<std::string> names;
  std::set<int> labels;
  for (const PickableObject& obj : config.pickable_objects) {
    validate(obj);
    if (!names.insert(obj.name).second) {
      throw ValidationError("Object name " + obj.name + " already exists.");
    }
    if (!labels.insert(obj.label).second) {
      throw ValidationError("Object label " + std::to_string(obj.label) + " already exists.");
    }
  }
}

}  // namespace copick
