// Picks — a set of point annotations for one object. (copick.models.CopickPicks)
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_PICKS_H
#define COPICK_PICKS_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "copick/export.h"
#include "copick/fwd.h"
#include "copick/types.h"

namespace copick {

class COPICK_API Picks {
 public:
  Picks() = default;
  explicit Picks(std::shared_ptr<detail::PicksImpl> impl) : impl_(std::move(impl)) {}

  bool valid() const { return static_cast<bool>(impl_); }
  explicit operator bool() const { return valid(); }

  const std::string& object_name() const;  // pickable_object_name
  const std::string& user_id() const;
  const std::string& session_id() const;
  bool from_tool() const;  // session_id == "0"
  bool from_user() const;
  Run run() const;

  /// The points (lazily loaded from storage on first access).
  const std::vector<Point>& points() const;
  /// Replace the points in memory (call store() to persist).
  void set_points(std::vector<Point> points) const;
  /// Persist the current points to storage.
  void store() const;
  /// Reload the points from storage.
  void refresh() const;

  const std::shared_ptr<detail::PicksImpl>& impl() const { return impl_; }

 private:
  std::shared_ptr<detail::PicksImpl> impl_;
};

}  // namespace copick

#endif  // COPICK_PICKS_H
