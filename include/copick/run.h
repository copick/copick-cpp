// Run — a handle to one experiment run (a location on the sample).
// (copick.models.CopickRun)
//
// PUBLIC HEADER — must compile under -std=c++11. A Run is a lightweight handle around a
// shared_ptr to its private implementation (the pImpl firewall); an empty/default Run is
// "invalid" (valid() == false), used where copick returns None.
#ifndef COPICK_RUN_H
#define COPICK_RUN_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "copick/export.h"
#include "copick/fwd.h"
#include "copick/mesh.h"
#include "copick/picks.h"
#include "copick/segmentation.h"
#include "copick/voxel_spacing.h"

namespace copick {

class COPICK_API Run {
 public:
  Run() = default;
  explicit Run(std::shared_ptr<detail::RunImpl> impl) : impl_(std::move(impl)) {}

  bool valid() const { return static_cast<bool>(impl_); }
  explicit operator bool() const { return valid(); }

  /// The run's name (its directory name, e.g. "TS_001").
  const std::string& name() const;

  /// The project root this run belongs to.
  Root root() const;

  /// Voxel spacings (lazily enumerated, sorted by size).
  std::vector<VoxelSpacing> voxel_spacings() const;
  /// Get a voxel spacing by size; invalid handle if absent. Sizes are matched at 3-decimal
  /// precision (copick rounds voxel spacings to the third decimal place).
  VoxelSpacing get_voxel_spacing(double voxel_size) const;
  /// Create a new voxel spacing (written to the overlay).
  VoxelSpacing new_voxel_spacing(double voxel_size, bool exist_ok = false);

  // --- Picks ---
  std::vector<Picks> picks() const;
  /// Filter picks (empty argument = no filter on that field).
  std::vector<Picks> get_picks(const std::string& object_name = "", const std::string& user_id = "",
                               const std::string& session_id = "") const;
  /// Create a new (empty) picks set, written to the overlay. `user_id` defaults to the
  /// config user_id. Throws if the object is unknown or the picks already exist (unless
  /// exist_ok).
  Picks new_picks(const std::string& object_name, const std::string& session_id,
                  const std::string& user_id = "", bool exist_ok = false);

  // --- Segmentations ---
  std::vector<Segmentation> segmentations() const;
  /// Filter segmentations (empty argument = no filter on that field).
  std::vector<Segmentation> get_segmentations(const std::string& user_id = "",
                                              const std::string& session_id = "",
                                              const std::string& name = "") const;
  /// Create a new segmentation, written to the overlay.
  Segmentation new_segmentation(double voxel_size, const std::string& name,
                                const std::string& session_id, bool is_multilabel,
                                const std::string& user_id = "", bool exist_ok = false);

  // --- Meshes ---
  std::vector<Mesh> meshes() const;
  /// Filter meshes (empty argument = no filter on that field).
  std::vector<Mesh> get_meshes(const std::string& object_name = "", const std::string& user_id = "",
                               const std::string& session_id = "") const;
  /// Create a new mesh, written to the overlay on store().
  Mesh new_mesh(const std::string& object_name, const std::string& session_id,
                const std::string& user_id = "", bool exist_ok = false);

  /// Internal: access the private implementation.
  const std::shared_ptr<detail::RunImpl>& impl() const { return impl_; }

 private:
  std::shared_ptr<detail::RunImpl> impl_;
};

}  // namespace copick

#endif  // COPICK_RUN_H
