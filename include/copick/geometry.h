// Geometry — a simple triangle mesh (vertices + triangular faces), the payload of a
// CopickMesh. Coordinates are in angstrom.
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_GEOMETRY_H
#define COPICK_GEOMETRY_H

#include <array>
#include <cstdint>
#include <vector>

namespace copick {

struct Geometry {
  std::vector<std::array<float, 3>> vertices;       // (x, y, z) per vertex
  std::vector<std::array<std::uint32_t, 3>> faces;  // vertex indices per triangle
};

}  // namespace copick

#endif  // COPICK_GEOMETRY_H
