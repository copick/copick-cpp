// Segmentation — a dense label volume. (copick.models.CopickSegmentation)
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_SEGMENTATION_H
#define COPICK_SEGMENTATION_H

#include <memory>
#include <string>
#include <utility>

#include "copick/array.h"
#include "copick/export.h"
#include "copick/fwd.h"

namespace copick {

class COPICK_API Segmentation {
 public:
  Segmentation() = default;
  explicit Segmentation(std::shared_ptr<detail::SegmentationImpl> impl) : impl_(std::move(impl)) {}

  bool valid() const { return static_cast<bool>(impl_); }
  explicit operator bool() const { return valid(); }

  const std::string& name() const;
  const std::string& user_id() const;
  const std::string& session_id() const;
  bool is_multilabel() const;
  double voxel_size() const;
  bool from_tool() const;  // session_id == "0"
  bool from_user() const;
  Run run() const;

  /// Read a region (default: whole array) at the given pyramid level.
  Array3D to_array(const Region& region = Region(), int level = 0) const;
  /// Write the segmentation (single level by default).
  void from_array(const Array3D& data, int levels = 1);
  /// Write a region into an existing level.
  void set_region(const Array3D& data, const Region& region, int level = 0);

  const std::shared_ptr<detail::SegmentationImpl>& impl() const { return impl_; }

 private:
  std::shared_ptr<detail::SegmentationImpl> impl_;
};

}  // namespace copick

#endif  // COPICK_SEGMENTATION_H
