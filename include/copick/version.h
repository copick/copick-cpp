// copick-cpp — C++ port of the copick cryo-ET dataset API.
//
// PUBLIC HEADER — must compile under -std=c++11 (see tests/header_guard). Do not use
// C++14/17/20 syntax here, and do not include any third-party dependency headers
// (tensorstore, reflect-cpp, tinygltf): those live behind the pImpl firewall in src/.
#ifndef COPICK_VERSION_H
#define COPICK_VERSION_H

#include "copick/export.h"

// Version numbers are maintained by release-please (do not edit the values by hand).
#define COPICK_VERSION_MAJOR 0         // x-release-please-major
#define COPICK_VERSION_MINOR 2         // x-release-please-minor
#define COPICK_VERSION_PATCH 0         // x-release-please-patch
#define COPICK_VERSION_STRING "0.2.0"  // x-release-please-version

namespace copick {

/// Returns the copick-cpp library version, e.g. "0.1.0".
COPICK_API const char* version();

}  // namespace copick

#endif  // COPICK_VERSION_H
