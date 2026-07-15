#include "io/kvstore.hpp"

#include <tensorstore/context.h>
#include <tensorstore/kvstore/key_range.h>
#include <tensorstore/kvstore/kvstore.h>
#include <tensorstore/kvstore/operations.h>

#include <absl/strings/cord.h>
#include <nlohmann/json.hpp>

#include <map>
#include <stdexcept>
#include <utility>

#include "copick/errors.h"
#include "io/context.hpp"

namespace ts = tensorstore;
using json = ::nlohmann::json;

namespace copick {
namespace io {

namespace {

std::string url_to_path(const std::string& url) {
  const auto pos = url.find("://");
  if (pos == std::string::npos) return url;
  std::string rest = url.substr(pos + 3);
  if (rest.empty()) return "/";
  return rest[0] == '/' ? rest : "/" + rest;
}

std::string join_key(const std::string& base, const std::string& key) {
  if (key.empty()) return base;
  if (base.empty()) return key;
  if (base.back() == '/') return base + key;
  return base + "/" + key;
}

// tensorstore's file/s3 kvstore treats "path" as a literal key prefix and appends keys
// WITHOUT inserting a separator, so a store rooted at "/a/b" turns key "c" into "/a/bc".
// Root paths must therefore end with "/" (empty prefixes are left empty).
std::string as_prefix(std::string p) {
  if (!p.empty() && p.back() != '/') p += '/';
  return p;
}

// Collapse runs of '/' to a single '/'. tensorstore's file kvstore rejects paths
// containing "//", which are easy to produce by joining copick path segments.
std::string collapse_slashes(const std::string& p) {
  std::string out;
  out.reserve(p.size());
  bool prev_slash = false;
  for (char c : p) {
    if (c == '/') {
      if (prev_slash) continue;
      prev_slash = true;
    } else {
      prev_slash = false;
    }
    out.push_back(c);
  }
  return out;
}

}  // namespace

struct KvStore::Impl {
  ts::kvstore::KvStore store;
  std::string driver;     // "file" or "s3"
  std::string base_path;  // filesystem path, or S3 key prefix
  std::string bucket;     // S3 only
  json fs_args;           // extra backend kwargs
  std::string root_url;

  json base_kvstore_spec(const std::string& key) const {
    if (driver == "s3") {
      json j = {
          {"driver", "s3"}, {"bucket", bucket}, {"path", as_prefix(join_key(base_path, key))}};
      for (auto it = fs_args.begin(); it != fs_args.end(); ++it) j[it.key()] = it.value();
      return j;
    }
    return json{{"driver", "file"},
                {"path", as_prefix(collapse_slashes(join_key(base_path, key)))}};
  }
};

KvStore KvStore::open(const std::string& url, const std::string& fs_args_json) {
  auto impl = std::make_shared<Impl>();
  impl->root_url = url;
  try {
    impl->fs_args = fs_args_json.empty() ? json::object() : json::parse(fs_args_json);
  } catch (const std::exception& e) {
    throw ValidationError(std::string("Invalid fs_args JSON: ") + e.what());
  }

  std::string scheme;
  const auto pos = url.find("://");
  if (pos != std::string::npos) scheme = url.substr(0, pos);

  if (scheme == "s3") {
    const std::string rest = url.substr(pos + 3);
    const auto slash = rest.find('/');
    impl->driver = "s3";
    impl->bucket = rest.substr(0, slash);
    impl->base_path = (slash == std::string::npos) ? "" : rest.substr(slash + 1);
  } else {
    impl->driver = "file";
    impl->base_path = url_to_path(url);
  }

  auto opened = ts::kvstore::Open(impl->base_kvstore_spec(""), shared_context()).result();
  if (!opened.ok()) {
    throw Error("Failed to open kvstore '" + url + "': " + opened.status().ToString());
  }
  impl->store = *std::move(opened);

  KvStore kv;
  kv.impl_ = std::move(impl);
  return kv;
}

bool KvStore::exists(const std::string& key) const {
  auto r = ts::kvstore::Read(impl_->store, key).result();
  if (!r.ok()) throw Error("kvstore read failed for '" + key + "': " + r.status().ToString());
  return r->state == ts::kvstore::ReadResult::kValue;
}

std::optional<std::string> KvStore::read(const std::string& key) const {
  auto r = ts::kvstore::Read(impl_->store, key).result();
  if (!r.ok()) throw Error("kvstore read failed for '" + key + "': " + r.status().ToString());
  if (r->state != ts::kvstore::ReadResult::kValue) return std::nullopt;
  return std::string(r->value);
}

void KvStore::write(const std::string& key, const std::string& bytes) {
  auto r = ts::kvstore::Write(impl_->store, key, absl::Cord(bytes)).result();
  if (!r.ok()) throw Error("kvstore write failed for '" + key + "': " + r.status().ToString());
}

void KvStore::remove(const std::string& key) {
  auto r = ts::kvstore::Delete(impl_->store, key).result();
  if (!r.ok()) throw Error("kvstore delete failed for '" + key + "': " + r.status().ToString());
}

void KvStore::remove_prefix(const std::string& prefix) {
  auto r = ts::kvstore::DeleteRange(impl_->store, ts::KeyRange::Prefix(prefix)).result();
  if (!r.ok()) {
    throw Error("kvstore delete-range failed for '" + prefix + "': " + r.status().ToString());
  }
}

std::vector<std::string> KvStore::list(const std::string& prefix) const {
  ts::kvstore::ListOptions opts;
  opts.range = ts::KeyRange::Prefix(prefix);
  auto entries = ts::kvstore::ListFuture(impl_->store, opts).result();
  if (!entries.ok()) throw Error("kvstore list failed: " + entries.status().ToString());
  std::vector<std::string> out;
  out.reserve(entries->size());
  for (const auto& e : *entries) out.push_back(e.key);
  return out;
}

std::vector<std::string> KvStore::list_children(const std::string& prefix,
                                                std::vector<bool>* is_dir) const {
  // Normalize prefix to end with '/' (unless empty) so segment extraction is clean.
  std::string pfx = prefix;
  if (!pfx.empty() && pfx.back() != '/') pfx += '/';

  // std::map -> deterministic, alphabetically-sorted children (tensorstore's List order
  // is unspecified / filesystem-dependent). A child is a "directory" if any key under it
  // had a further path segment.
  std::map<std::string, bool> children;  // name -> is_dir
  for (const std::string& key : list(pfx)) {
    if (key.size() <= pfx.size()) continue;
    const std::string rest = key.substr(pfx.size());
    const auto slash = rest.find('/');
    const std::string child = (slash == std::string::npos) ? rest : rest.substr(0, slash);
    if (child.empty()) continue;
    children[child] = children[child] || (slash != std::string::npos);
  }

  std::vector<std::string> names;
  std::vector<bool> dirs;
  names.reserve(children.size());
  for (const auto& kv : children) {
    names.push_back(kv.first);
    dirs.push_back(kv.second);
  }
  if (is_dir) *is_dir = std::move(dirs);
  return names;
}

const std::string& KvStore::root_url() const {
  return impl_->root_url;
}

std::string KvStore::kvstore_spec(const std::string& key) const {
  return impl_->base_kvstore_spec(key).dump();
}

}  // namespace io
}  // namespace copick
