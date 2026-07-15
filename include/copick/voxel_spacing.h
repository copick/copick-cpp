// VoxelSpacing — groups tomograms of one resolution. (copick.models.CopickVoxelSpacing)
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_VOXEL_SPACING_H
#define COPICK_VOXEL_SPACING_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "copick/export.h"
#include "copick/fwd.h"
#include "copick/tomogram.h"

namespace copick {

class COPICK_API VoxelSpacing {
 public:
  VoxelSpacing() = default;
  explicit VoxelSpacing(std::shared_ptr<detail::VoxelSpacingImpl> impl) : impl_(std::move(impl)) {}

  bool valid() const { return static_cast<bool>(impl_); }
  explicit operator bool() const { return valid(); }

  double voxel_size() const;
  Run run() const;  // parent

  std::vector<Tomogram> tomograms() const;
  Tomogram get_tomogram(const std::string& tomo_type) const;
  Tomogram new_tomogram(const std::string& tomo_type, bool exist_ok = false);

  const std::shared_ptr<detail::VoxelSpacingImpl>& impl() const { return impl_; }

 private:
  std::shared_ptr<detail::VoxelSpacingImpl> impl_;
};

}  // namespace copick

#endif  // COPICK_VOXEL_SPACING_H
