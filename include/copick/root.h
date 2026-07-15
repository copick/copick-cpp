// Root — the entry point to a copick project. (copick.models.CopickRoot)
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_ROOT_H
#define COPICK_ROOT_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "copick/config.h"
#include "copick/export.h"
#include "copick/fwd.h"
#include "copick/object.h"
#include "copick/run.h"

namespace copick {

class COPICK_API Root {
 public:
  Root() = default;
  explicit Root(std::shared_ptr<detail::RootImpl> impl) : impl_(std::move(impl)) {}

  bool valid() const { return static_cast<bool>(impl_); }
  explicit operator bool() const { return valid(); }

  /// The project configuration.
  const CopickConfig& config() const;

  /// All runs (lazily enumerated and cached; sorted by name).
  std::vector<Run> runs() const;

  /// Get a run by name; returns an invalid Run if it does not exist.
  Run get_run(const std::string& name) const;

  /// Create a new run (written to the overlay). Throws if it exists unless exist_ok.
  Run new_run(const std::string& name, bool exist_ok = false);

  /// Delete a run (from the overlay). No-op if it does not exist.
  void delete_run(const std::string& name);

  /// The pickable objects defined in the config.
  std::vector<Object> pickable_objects() const;
  /// Get a pickable object by name; invalid handle if not defined.
  Object get_object(const std::string& name) const;

  /// Drop cached enumerations, forcing re-query on next access.
  void refresh();

  /// Write the configuration to a JSON file.
  void save_config(const std::string& path) const;

  /// Internal: access the private implementation.
  const std::shared_ptr<detail::RootImpl>& impl() const { return impl_; }

 private:
  std::shared_ptr<detail::RootImpl> impl_;
};

// --- Entry points (mirror copick.from_file / from_string / new_config) ------------

/// Load a project from a config file on disk.
COPICK_API Root from_file(const std::string& config_path);

/// Load a project from a config JSON string.
COPICK_API Root from_string(const std::string& config_json);

/// Create a fresh filesystem project: write a minimal config to `config_path` pointing at
/// `overlay_root`, then open it.
COPICK_API Root new_config(const std::string& config_path, const std::string& overlay_root,
                           const std::string& project_name = "copick project");

}  // namespace copick

#endif  // COPICK_ROOT_H
