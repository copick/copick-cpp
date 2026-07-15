// Core copick data types (the metadata carried by each entity).
//
// These mirror the pydantic models in copick's models.py. They are plain C++11
// structs so they serve as the ABI-stable data representation across both the C++11
// core API and the (later) C++20 modern layer. JSON (de)serialization lives in src/
// via reflect-cpp; these headers never mention it.
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_TYPES_H
#define COPICK_TYPES_H

#include <array>
#include <string>
#include <vector>

#include "copick/export.h"
#include "copick/optional.h"

namespace copick {

/// RGBA color, each channel in [0, 255]. Mirrors copick's 4-tuple.
using Color = std::array<int, 4>;

/// Row-major 4x4 transformation matrix (object-space -> tomogram-space).
using Matrix4 = std::array<std::array<double, 4>, 4>;

/// The 4x4 identity matrix.
inline Matrix4 identity_matrix4() {
  Matrix4 m = {};
  m[0][0] = 1.0;
  m[1][1] = 1.0;
  m[2][2] = 1.0;
  m[3][3] = 1.0;
  return m;
}

/// A location in 3D space (angstrom coordinates).
struct Location {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

/// A picked point: location + orientation + score + instance id.
/// (copick.models.CopickPoint)
struct Point {
  Location location;
  Matrix4 transformation = identity_matrix4();
  int instance_id = 0;
  double score = 1.0;
};

/// Definition of a pickable object. (copick.models.PickableObject)
///
/// `metadata` holds the free-form `Dict[str, Any]` as raw JSON object text
/// (default "{}"); parse it with your JSON library of choice.
struct PickableObject {
  std::string name;
  bool is_particle = false;
  int label = 1;  // 0 is reserved for background
  Color color = {{100, 100, 100, 255}};
  optional<std::string> emdb_id;
  optional<std::string> pdb_id;
  optional<std::string> identifier;  // ontology/database id (aka go_id)
  optional<double> map_threshold;
  optional<double> radius;
  std::string metadata = "{}";  // raw JSON object
};

/// A collection of points for one pickable object in one run.
/// (copick.models.CopickPicksFile)
struct CopickPicksFile {
  std::string pickable_object_name;
  std::string user_id;
  std::string session_id;  // "0" => tool-generated
  optional<std::string> run_name;
  optional<double> voxel_spacing;
  std::string unit = "angstrom";
  std::vector<Point> points;
  bool trust_orientation = true;
};

}  // namespace copick

#endif  // COPICK_TYPES_H
