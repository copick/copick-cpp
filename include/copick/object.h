// Object — a pickable object definition, optionally with a density map.
// (copick.models.CopickObject)
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_OBJECT_H
#define COPICK_OBJECT_H

#include <memory>
#include <string>
#include <utility>

#include "copick/array.h"
#include "copick/export.h"
#include "copick/fwd.h"
#include "copick/types.h"

namespace copick {

class COPICK_API Object {
 public:
  Object() = default;
  explicit Object(std::shared_ptr<detail::ObjectImpl> impl) : impl_(std::move(impl)) {}

  bool valid() const { return static_cast<bool>(impl_); }
  explicit operator bool() const { return valid(); }

  const std::string& name() const;
  bool is_particle() const;
  int label() const;
  const Color& color() const;
  /// The full pickable-object metadata (pdb_id, emdb_id, radius, ...).
  const PickableObject& meta() const;

  /// Whether a density map (Objects/<name>.zarr) exists for this particle object.
  bool has_density() const;
  /// Read the density map region (particle objects only).
  Array3D to_array(const Region& region = Region(), int level = 0) const;
  /// Write a density map for this particle object.
  void from_array(const Array3D& data, double voxel_size, int levels = 1);

  const std::shared_ptr<detail::ObjectImpl>& impl() const { return impl_; }

 private:
  std::shared_ptr<detail::ObjectImpl> impl_;
};

}  // namespace copick

#endif  // COPICK_OBJECT_H
