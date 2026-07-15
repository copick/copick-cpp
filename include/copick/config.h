// The copick project configuration. (copick.models.CopickConfig)
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_CONFIG_H
#define COPICK_CONFIG_H

#include <string>
#include <vector>

#include "copick/export.h"
#include "copick/optional.h"
#include "copick/types.h"

namespace copick {

/// Project configuration: the pickable objects plus identity/index metadata.
///
/// `config_type` selects the storage backend ("filesystem", "cryoet_data_portal",
/// "mlcroissant"); only "filesystem" is supported in this port.
struct CopickConfig {
  std::string name = "CoPick";
  std::string description = "Let's CoPick!";
  std::string version = "0.2.0";
  std::string config_type = "filesystem";
  std::vector<PickableObject> pickable_objects;
  optional<std::string> user_id;
  optional<std::string> session_id;
  optional<std::vector<std::string>> runs;

  // Filesystem backend fields (config_type == "filesystem").
  // Roots are fsspec-style URLs, e.g. "local:///abs/path" or "s3://bucket/prefix".
  // If static_root is unset, the overlay acts as the sole (writable) source.
  // *_fs_args hold the fsspec kwargs as raw JSON object text (default "{}").
  std::string overlay_root;
  optional<std::string> static_root;
  std::string overlay_fs_args = "{}";
  std::string static_fs_args = "{}";
};

}  // namespace copick

#endif  // COPICK_CONFIG_H
