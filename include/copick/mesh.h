// Mesh — a mesh annotation (stored as GLB). (copick.models.CopickMesh)
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_MESH_H
#define COPICK_MESH_H

#include <memory>
#include <string>
#include <utility>

#include "copick/export.h"
#include "copick/fwd.h"
#include "copick/geometry.h"

namespace copick {

class COPICK_API Mesh {
 public:
  Mesh() = default;
  explicit Mesh(std::shared_ptr<detail::MeshImpl> impl) : impl_(std::move(impl)) {}

  bool valid() const { return static_cast<bool>(impl_); }
  explicit operator bool() const { return valid(); }

  const std::string& object_name() const;
  const std::string& user_id() const;
  const std::string& session_id() const;
  bool from_tool() const;  // session_id == "0"
  bool from_user() const;
  Run run() const;

  /// The geometry (lazily loaded from the GLB on first access).
  const Geometry& mesh() const;
  /// Replace the geometry in memory (call store() to persist).
  void set_mesh(Geometry geometry) const;
  /// Persist the geometry as a GLB.
  void store() const;
  /// Reload the geometry from storage.
  void refresh() const;

  const std::shared_ptr<detail::MeshImpl>& impl() const { return impl_; }

 private:
  std::shared_ptr<detail::MeshImpl> impl_;
};

}  // namespace copick

#endif  // COPICK_MESH_H
