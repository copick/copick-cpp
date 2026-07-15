// Exception hierarchy for copick.
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_ERRORS_H
#define COPICK_ERRORS_H

#include <stdexcept>
#include <string>

#include "copick/export.h"

namespace copick {

/// Base class for all copick exceptions.
class COPICK_API Error : public std::runtime_error {
 public:
  explicit Error(const std::string& what) : std::runtime_error(what) {}
};

/// Raised when a value fails validation (mirrors pydantic ValidationError / ValueError).
class COPICK_API ValidationError : public Error {
 public:
  explicit ValidationError(const std::string& what) : Error(what) {}
};

/// Raised when a requested entity does not exist.
class COPICK_API NotFoundError : public Error {
 public:
  explicit NotFoundError(const std::string& what) : Error(what) {}
};

/// Raised when attempting to mutate a read-only (static) entity.
class COPICK_API PermissionError : public Error {
 public:
  explicit PermissionError(const std::string& what) : Error(what) {}
};

}  // namespace copick

#endif  // COPICK_ERRORS_H
