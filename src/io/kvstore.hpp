// KvStore — the storage layer, backed entirely by tensorstore's kvstore (file:// and
// s3:// uniformly). copick-cpp only does zarr-agnostic byte get/put/list here (pick
// .json, mesh .glb, group .zattrs) and directory enumeration; all Zarr chunk/compression
// logic stays inside tensorstore (see zarr.hpp).
//
// Keys are '/'-separated paths relative to the store root. pImpl keeps tensorstore
// headers out of everything that includes this. Compiled at C++20, only under
// COPICK_ENABLE_ZARR.
#ifndef COPICK_IO_KVSTORE_HPP
#define COPICK_IO_KVSTORE_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace copick {
namespace io {

class KvStore {
 public:
  KvStore() = default;

  /// Open a store for a copick root URL: "local://"/"file://"/plain path -> filesystem;
  /// "s3://bucket/prefix" -> S3. `fs_args_json` carries backend kwargs as raw JSON
  /// (S3 endpoint/credentials, etc.).
  static KvStore open(const std::string& url, const std::string& fs_args_json = "{}");

  /// True if the exact key exists (a stored object, not a directory prefix).
  bool exists(const std::string& key) const;

  /// Read an entire object; nullopt if the key is missing.
  std::optional<std::string> read(const std::string& key) const;

  /// Write (overwrite) an entire object.
  void write(const std::string& key, const std::string& bytes);

  /// Delete a single key. No error if absent.
  void remove(const std::string& key);

  /// Delete every key under `prefix` (recursive). No error if none.
  void remove_prefix(const std::string& prefix);

  /// All (leaf) keys under `prefix`, relative to the root.
  std::vector<std::string> list(const std::string& prefix) const;

  /// Immediate child names under `prefix` (first path segment after the prefix, deduped).
  /// If `is_dir` is non-null it is filled parallel to the result: true when that child had
  /// further path segments (i.e. is a "directory").
  std::vector<std::string> list_children(const std::string& prefix,
                                         std::vector<bool>* is_dir = nullptr) const;

  /// The normalized root URL.
  const std::string& root_url() const;

  /// A tensorstore kvstore spec (JSON string) addressing `key` under this root, for
  /// nesting inside a Zarr driver spec (see zarr.hpp).
  std::string kvstore_spec(const std::string& key) const;

  bool valid() const { return static_cast<bool>(impl_); }

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace io
}  // namespace copick

#endif  // COPICK_IO_KVSTORE_HPP
