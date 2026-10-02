// Internal reflect-cpp DTOs mirroring copick's on-disk JSON, plus conversions to/from
// the public C++11 model structs. Compiled at C++20; never included by public headers.
//
// Field names here are the exact JSON keys copick reads/writes (e.g. `transformation_`
// with the trailing underscore, matching pydantic's model_dump()). Every field that may
// be absent is std::optional, because reflect-cpp errors on a missing non-optional field
// even when it has a default member initializer.
#ifndef COPICK_IO_DTO_HPP
#define COPICK_IO_DTO_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "copick/config.h"
#include "copick/errors.h"
#include "copick/types.h"

namespace copick {
namespace io {

// --- Free-form JSON (Dict[str, Any]) <-> raw JSON string --------------------------
inline std::string generic_to_string(const rfl::Generic& g) {
  return rfl::json::write(g);
}

inline rfl::Generic string_to_generic(const std::string& s) {
  const std::string src = s.empty() ? "{}" : s;
  const auto r = rfl::json::read<rfl::Generic>(src);
  if (!r) throw ValidationError("Invalid JSON object: " + r.error().what());
  return r.value();
}

// --- PickableObject ---------------------------------------------------------------
struct PickableObjectDTO {
  std::string name;
  bool is_particle = false;
  std::optional<int> label;
  std::optional<std::array<int, 4>> color;
  std::optional<std::string> emdb_id;
  std::optional<std::string> pdb_id;
  std::optional<std::string> identifier;
  std::optional<std::string> go_id;  // legacy alias for identifier (read-only)
  std::optional<double> map_threshold;
  std::optional<double> radius;
  std::optional<rfl::Generic> metadata;
};

inline PickableObject from_dto(const PickableObjectDTO& d) {
  PickableObject o;
  o.name = d.name;
  o.is_particle = d.is_particle;
  o.label = d.label.value_or(1);
  o.color = d.color.value_or(Color{{100, 100, 100, 255}});
  if (d.emdb_id) o.emdb_id = *d.emdb_id;
  if (d.pdb_id) o.pdb_id = *d.pdb_id;
  if (d.identifier)
    o.identifier = *d.identifier;
  else if (d.go_id)
    o.identifier = *d.go_id;  // AliasChoices("go_id", "identifier")
  if (d.map_threshold) o.map_threshold = *d.map_threshold;
  if (d.radius) o.radius = *d.radius;
  o.metadata = d.metadata ? generic_to_string(*d.metadata) : std::string("{}");
  return o;
}

inline PickableObjectDTO to_dto(const PickableObject& o) {
  PickableObjectDTO d;
  d.name = o.name;
  d.is_particle = o.is_particle;
  d.label = o.label;
  d.color = o.color;
  if (o.emdb_id) d.emdb_id = *o.emdb_id;
  if (o.pdb_id) d.pdb_id = *o.pdb_id;
  if (o.identifier) d.identifier = *o.identifier;
  if (o.map_threshold) d.map_threshold = *o.map_threshold;
  if (o.radius) d.radius = *o.radius;
  d.metadata = string_to_generic(o.metadata);
  return d;
}

// --- CopickConfig -----------------------------------------------------------------
struct ConfigDTO {
  std::optional<std::string> name;
  std::optional<std::string> description;
  std::optional<std::string> version;
  std::optional<std::string> config_type;
  std::vector<PickableObjectDTO> pickable_objects;
  std::optional<std::string> user_id;
  std::optional<std::string> session_id;
  std::optional<std::vector<std::string>> runs;
  std::optional<std::string> overlay_root;
  std::optional<std::string> static_root;
  std::optional<rfl::Generic> overlay_fs_args;
  std::optional<rfl::Generic> static_fs_args;
};

inline CopickConfig from_dto(const ConfigDTO& d) {
  CopickConfig c;
  c.name = d.name.value_or("CoPick");
  c.description = d.description.value_or("Let's CoPick!");
  c.version = d.version.value_or("0.2.0");
  c.config_type = d.config_type.value_or("filesystem");
  c.pickable_objects.reserve(d.pickable_objects.size());
  for (const auto& od : d.pickable_objects) c.pickable_objects.push_back(from_dto(od));
  if (d.user_id) c.user_id = *d.user_id;
  if (d.session_id) c.session_id = *d.session_id;
  if (d.runs) c.runs = *d.runs;
  c.overlay_root = d.overlay_root.value_or("");
  if (d.static_root) c.static_root = *d.static_root;
  c.overlay_fs_args = d.overlay_fs_args ? generic_to_string(*d.overlay_fs_args) : std::string("{}");
  c.static_fs_args = d.static_fs_args ? generic_to_string(*d.static_fs_args) : std::string("{}");
  return c;
}

inline ConfigDTO to_dto(const CopickConfig& c) {
  ConfigDTO d;
  d.name = c.name;
  d.description = c.description;
  d.version = c.version;
  d.config_type = c.config_type;
  d.pickable_objects.reserve(c.pickable_objects.size());
  for (const auto& o : c.pickable_objects) d.pickable_objects.push_back(to_dto(o));
  if (c.user_id) d.user_id = *c.user_id;
  if (c.session_id) d.session_id = *c.session_id;
  if (c.runs) d.runs = *c.runs;
  if (!c.overlay_root.empty()) d.overlay_root = c.overlay_root;
  if (c.static_root) d.static_root = *c.static_root;
  d.overlay_fs_args = string_to_generic(c.overlay_fs_args);
  if (c.static_root) d.static_fs_args = string_to_generic(c.static_fs_args);
  return d;
}

// --- Point / CopickPicksFile ------------------------------------------------------
struct LocationDTO {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

struct PointDTO {
  LocationDTO location;
  std::optional<std::array<std::array<double, 4>, 4>> transformation_;
  std::optional<std::int64_t> instance_id;
  std::optional<double> score;
};

struct PicksFileDTO {
  std::string pickable_object_name;
  std::string user_id;
  std::string session_id;
  std::optional<std::string> run_name;
  std::optional<double> voxel_spacing;
  std::optional<std::string> unit;
  std::optional<std::vector<PointDTO>> points;
  std::optional<bool> trust_orientation;
};

inline Point from_dto(const PointDTO& d) {
  Point p;
  p.location.x = d.location.x;
  p.location.y = d.location.y;
  p.location.z = d.location.z;
  p.transformation = d.transformation_.value_or(identity_matrix4());
  p.instance_id = d.instance_id.value_or(0);
  p.score = d.score.value_or(1.0);
  return p;
}

inline PointDTO to_dto(const Point& p) {
  PointDTO d;
  d.location.x = p.location.x;
  d.location.y = p.location.y;
  d.location.z = p.location.z;
  d.transformation_ = p.transformation;
  d.instance_id = p.instance_id;
  d.score = p.score;
  return d;
}

inline CopickPicksFile from_dto(const PicksFileDTO& d) {
  CopickPicksFile f;
  f.pickable_object_name = d.pickable_object_name;
  f.user_id = d.user_id;
  f.session_id = d.session_id;
  if (d.run_name) f.run_name = *d.run_name;
  if (d.voxel_spacing) f.voxel_spacing = *d.voxel_spacing;
  f.unit = d.unit.value_or("angstrom");
  if (d.points) {
    f.points.reserve(d.points->size());
    for (const auto& pd : *d.points) f.points.push_back(from_dto(pd));
  }
  f.trust_orientation = d.trust_orientation.value_or(true);
  return f;
}

inline PicksFileDTO to_dto(const CopickPicksFile& f) {
  PicksFileDTO d;
  d.pickable_object_name = f.pickable_object_name;
  d.user_id = f.user_id;
  d.session_id = f.session_id;
  if (f.run_name) d.run_name = *f.run_name;
  if (f.voxel_spacing) d.voxel_spacing = *f.voxel_spacing;
  d.unit = f.unit;
  std::vector<PointDTO> pts;
  pts.reserve(f.points.size());
  for (const auto& p : f.points) pts.push_back(to_dto(p));
  d.points = std::move(pts);
  d.trust_orientation = f.trust_orientation;
  return d;
}

}  // namespace io
}  // namespace copick

#endif  // COPICK_IO_DTO_HPP
