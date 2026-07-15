#include <algorithm>
#include <fstream>
#include <set>
#include <utility>

#include "copick/errors.h"
#include "copick/root.h"
#include "copick/run.h"
#include "copick/serialize.h"
#include "detail/impl.hpp"
#include "io/kvstore.hpp"

namespace copick {

// ================================ detail impls ===================================
namespace detail {

const std::vector<std::string>& RootImpl::run_names() {
  if (!run_names_cache) {
    std::set<std::string> names;  // sorted + deduped
    for (const auto& c : overlay.list_children("ExperimentRuns")) {
      if (!c.empty() && c[0] != '.') names.insert(c);
    }
    if (!static_is_overlay) {
      for (const auto& c : static_store.list_children("ExperimentRuns")) {
        if (!c.empty() && c[0] != '.') names.insert(c);
      }
    }
    run_names_cache = std::vector<std::string>(names.begin(), names.end());
  }
  return *run_names_cache;
}

namespace {
bool run_present(const io::KvStore& store, const std::string& path) {
  return !store.list_children(path).empty();
}
}  // namespace

bool RunImpl::ensure(bool create) {
  bool exists = run_present(root->overlay, path());
  if (!root->static_is_overlay && !exists) exists = run_present(root->static_store, path());

  if (!exists && create) {
    root->overlay.write(path() + "/.meta", "meta");  // create marker (mirrors copick)
    return true;
  }
  return exists;
}

void RunImpl::remove() {
  root->overlay.remove_prefix(path());
}

namespace {
std::shared_ptr<RunImpl> make_run(const std::shared_ptr<RootImpl>& root, const std::string& name) {
  auto r = std::make_shared<RunImpl>();
  r->root = root;
  r->name = name;
  return r;
}
}  // namespace

}  // namespace detail

// ================================ Run handle =====================================
const std::string& Run::name() const {
  return impl_->name;
}

// ================================ Root handle ====================================
const CopickConfig& Root::config() const {
  return impl_->config;
}

std::vector<Run> Root::runs() const {
  std::vector<Run> out;
  const auto& names = impl_->run_names();
  out.reserve(names.size());
  for (const auto& name : names) out.push_back(Run(detail::make_run(impl_, name)));
  return out;
}

Run Root::get_run(const std::string& name) const {
  const auto& names = impl_->run_names();
  if (std::find(names.begin(), names.end(), name) == names.end()) return Run();
  return Run(detail::make_run(impl_, name));
}

Run Root::new_run(const std::string& name, bool exist_ok) {
  const auto& names = impl_->run_names();
  const bool present = std::find(names.begin(), names.end(), name) != names.end();
  if (present) {
    if (exist_ok) return get_run(name);
    throw ValidationError("Run name " + name + " already exists.");
  }
  auto run = detail::make_run(impl_, name);
  run->ensure(true);
  if (impl_->run_names_cache) {
    impl_->run_names_cache->push_back(name);
    std::sort(impl_->run_names_cache->begin(), impl_->run_names_cache->end());
  }
  return Run(run);
}

void Root::delete_run(const std::string& name) {
  Run r = get_run(name);
  if (!r.valid()) return;
  r.impl()->remove();
  if (impl_->run_names_cache) {
    auto& v = *impl_->run_names_cache;
    v.erase(std::remove(v.begin(), v.end(), name), v.end());
  }
}

void Root::refresh() {
  impl_->refresh();
}

void Root::save_config(const std::string& path) const {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) throw Error("Failed to open config for write: " + path);
  const std::string json = config_to_json(impl_->config);
  f.write(json.data(), static_cast<std::streamsize>(json.size()));
  if (!f) throw Error("Failed to write config: " + path);
}

// ================================ entry points ===================================
Root from_string(const std::string& config_json) {
  CopickConfig cfg = config_from_json(config_json);
  if (cfg.config_type != "filesystem") {
    throw ValidationError("copick-cpp supports only config_type 'filesystem' (got '" +
                          cfg.config_type + "').");
  }
  if (cfg.overlay_root.empty()) {
    throw ValidationError("filesystem config requires a non-empty overlay_root.");
  }

  auto impl = std::make_shared<detail::RootImpl>();
  impl->config = std::move(cfg);
  impl->overlay = io::KvStore::open(impl->config.overlay_root, impl->config.overlay_fs_args);
  if (impl->config.static_root) {
    impl->static_store = io::KvStore::open(*impl->config.static_root, impl->config.static_fs_args);
    impl->static_is_overlay = false;
  } else {
    impl->static_store = impl->overlay;
    impl->static_is_overlay = true;
  }
  return Root(impl);
}

Root from_file(const std::string& config_path) {
  std::ifstream f(config_path, std::ios::binary);
  if (!f) throw Error("Failed to open config file: " + config_path);
  std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return from_string(json);
}

Root new_config(const std::string& config_path, const std::string& overlay_root,
                const std::string& project_name) {
  CopickConfig cfg;
  cfg.name = project_name;
  cfg.config_type = "filesystem";
  cfg.overlay_root = overlay_root;
  cfg.overlay_fs_args = "{\"auto_mkdir\": true}";
  const std::string json = config_to_json(cfg);
  {
    std::ofstream f(config_path, std::ios::binary | std::ios::trunc);
    if (!f) throw Error("Failed to write config: " + config_path);
    f.write(json.data(), static_cast<std::streamsize>(json.size()));
  }
  return from_file(config_path);
}

}  // namespace copick
