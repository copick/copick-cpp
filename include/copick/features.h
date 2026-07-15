// Features — a feature map computed from a tomogram. (copick.models.CopickFeatures)
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_FEATURES_H
#define COPICK_FEATURES_H

#include <memory>
#include <string>
#include <utility>

#include "copick/array.h"
#include "copick/export.h"
#include "copick/fwd.h"

namespace copick {

class COPICK_API Features {
 public:
  Features() = default;
  explicit Features(std::shared_ptr<detail::FeaturesImpl> impl) : impl_(std::move(impl)) {}

  bool valid() const { return static_cast<bool>(impl_); }
  explicit operator bool() const { return valid(); }

  const std::string& feature_type() const;
  const std::string& tomo_type() const;
  Tomogram tomogram() const;  // parent

  /// Read a region (default: whole array) at the given pyramid level.
  Array3D numpy(const Region& region = Region(), int level = 0) const;
  /// Write the feature map (single level by default).
  void from_numpy(const Array3D& data, int levels = 1);
  /// Write a region into an existing level.
  void set_region(const Array3D& data, const Region& region, int level = 0);

  const std::shared_ptr<detail::FeaturesImpl>& impl() const { return impl_; }

 private:
  std::shared_ptr<detail::FeaturesImpl> impl_;
};

}  // namespace copick

#endif  // COPICK_FEATURES_H
