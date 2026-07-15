// Tomogram — a 3D reconstruction at one voxel spacing. (copick.models.CopickTomogram)
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_TOMOGRAM_H
#define COPICK_TOMOGRAM_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "copick/array.h"
#include "copick/export.h"
#include "copick/features.h"
#include "copick/fwd.h"

namespace copick {

class COPICK_API Tomogram {
 public:
  Tomogram() = default;
  explicit Tomogram(std::shared_ptr<detail::TomogramImpl> impl) : impl_(std::move(impl)) {}

  bool valid() const { return static_cast<bool>(impl_); }
  explicit operator bool() const { return valid(); }

  const std::string& tomo_type() const;
  double voxel_size() const;           // from the parent voxel spacing
  VoxelSpacing voxel_spacing() const;  // parent

  /// Read a region (default: whole array) at the given pyramid level.
  Array3D numpy(const Region& region = Region(), int level = 0) const;
  /// Write the tomogram (single level by default; voxel size from the parent spacing).
  void from_numpy(const Array3D& data, int levels = 1);
  /// Write a region into an existing level.
  void set_region(const Array3D& data, const Region& region, int level = 0);

  std::vector<Features> features() const;
  Features get_features(const std::string& feature_type) const;
  Features new_features(const std::string& feature_type, bool exist_ok = false);

  const std::shared_ptr<detail::TomogramImpl>& impl() const { return impl_; }

 private:
  std::shared_ptr<detail::TomogramImpl> impl_;
};

}  // namespace copick

#endif  // COPICK_TOMOGRAM_H
