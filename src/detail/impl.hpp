// Private backend implementations behind the copick handle classes. Compiled at C++20,
// only under COPICK_ENABLE_ZARR. Ownership is up-the-tree via shared_ptr (a child impl
// keeps its ancestors alive, matching Python); the root owns config + storage and caches
// only lightweight enumerations, so there are no reference cycles.
#ifndef COPICK_DETAIL_IMPL_HPP
#define COPICK_DETAIL_IMPL_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "copick/array.h"
#include "copick/config.h"
#include "copick/geometry.h"
#include "copick/types.h"
#include "io/kvstore.hpp"

namespace copick {
namespace detail {

// --- shared helpers (defined in image.cpp) ---------------------------------------
/// Format a voxel size the way copick names directories: f"{v:.3f}" (e.g. 10.0 -> "10.000").
std::string format_voxel_size(double voxel_size);
/// The KvStore to read an entity from (static for read-only in a two-source project).
io::KvStore& read_store(RootImpl* r, bool read_only);
/// Write an Array3D as an OME-Zarr group (single level): `.zgroup` + `.zattrs` + level "0".
void write_ome_array(io::KvStore& store, const std::string& zpath, const Array3D& data,
                     double voxel_size);
/// Read a level of an OME-Zarr group into an Array3D.
Array3D read_ome_level(io::KvStore& store, const std::string& zpath, const Region& region,
                       int level);
/// Split a string on a delimiter (defined in annotation.cpp).
std::vector<std::string> split_on(const std::string& s, char delim);
/// Whether `name` is a configured pickable object.
bool is_pickable(RootImpl* r, const std::string& name);
/// Effective user id (supplied, else config user_id); throws if neither is set.
std::string resolve_user(RootImpl* r, const std::string& user_id, const char* fn);

class RootImpl {
 public:
  CopickConfig config;
  io::KvStore overlay;
  io::KvStore static_store;  // aliases `overlay` when static_is_overlay
  bool static_is_overlay = true;

  std::optional<std::vector<std::string>> run_names_cache;

  /// Enumerated run names (merged static+overlay, hidden filtered, sorted+deduped).
  const std::vector<std::string>& run_names();
  void refresh() { run_names_cache.reset(); }
};

class RunImpl {
 public:
  std::shared_ptr<RootImpl> root;
  std::string name;

  /// Relative key of the run directory.
  std::string path() const { return "ExperimentRuns/" + name; }

  /// Existence check (static or overlay); optionally create the overlay marker.
  bool ensure(bool create);

  /// Delete the run subtree from the overlay.
  void remove();
};

class VoxelSpacingImpl {
 public:
  std::shared_ptr<RunImpl> run;
  double voxel_size = 0.0;

  RootImpl* root() const { return run->root.get(); }
  std::string path() const { return run->path() + "/VoxelSpacing" + format_voxel_size(voxel_size); }

  bool ensure(bool create);
  void remove();
};

class TomogramImpl {
 public:
  std::shared_ptr<VoxelSpacingImpl> vs;
  std::string tomo_type;
  bool read_only = false;

  RootImpl* root() const { return vs->run->root.get(); }
  std::string stem() const { return vs->path() + "/" + tomo_type; }  // no ".zarr"
  std::string zarr_path() const { return stem() + ".zarr"; }

  void remove();
};

class FeaturesImpl {
 public:
  std::shared_ptr<TomogramImpl> tomo;
  std::string feature_type;
  bool read_only = false;

  RootImpl* root() const { return tomo->vs->run->root.get(); }
  std::string zarr_path() const { return tomo->stem() + "_" + feature_type + "_features.zarr"; }

  void remove();
};

class ObjectImpl {
 public:
  std::shared_ptr<RootImpl> root;
  PickableObject meta;
  bool read_only = false;

  std::string zarr_path() const { return "Objects/" + meta.name + ".zarr"; }
};

class PicksImpl {
 public:
  std::shared_ptr<RunImpl> run;
  CopickPicksFile file;  // object_name, user_id, session_id, points, ...
  bool read_only = false;
  bool loaded = false;  // points loaded from storage?

  RootImpl* root() const { return run->root.get(); }
  std::string path() const {
    return run->path() + "/Picks/" + file.user_id + "_" + file.session_id + "_" +
           file.pickable_object_name + ".json";
  }
  void load();
  void store();
  void remove();
};

class SegmentationImpl {
 public:
  std::shared_ptr<RunImpl> run;
  std::string user_id;
  std::string session_id;
  std::string name;
  bool is_multilabel = false;
  double voxel_size = 0.0;
  bool read_only = false;

  RootImpl* root() const { return run->root.get(); }
  std::string filename() const;
  std::string path() const { return run->path() + "/Segmentations/" + filename(); }
  void remove();
};

class MeshImpl {
 public:
  std::shared_ptr<RunImpl> run;
  std::string object_name;
  std::string user_id;
  std::string session_id;
  bool read_only = false;
  bool loaded = false;
  Geometry geom;

  RootImpl* root() const { return run->root.get(); }
  std::string path() const {
    return run->path() + "/Meshes/" + user_id + "_" + session_id + "_" + object_name + ".glb";
  }
  void load();
  void store();
  void remove();
};

}  // namespace detail
}  // namespace copick

#endif  // COPICK_DETAIL_IMPL_HPP
