// Image entities: VoxelSpacing -> Tomogram -> Features, plus the Run-level voxel-spacing
// accessors. Compiled at C++20, only under COPICK_ENABLE_ZARR.
#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "copick/errors.h"
#include "copick/features.h"
#include "copick/root.h"
#include "copick/run.h"
#include "copick/tomogram.h"
#include "copick/voxel_spacing.h"
#include "detail/impl.hpp"
#include "io/kvstore.hpp"
#include "io/ome.hpp"
#include "io/zarr.hpp"

namespace copick {
namespace detail {

std::string format_voxel_size(double voxel_size) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.3f", voxel_size);
  return std::string(buf);
}

bool VoxelSpacingImpl::ensure(bool create) {
  auto* r = root();
  const std::string key = path();
  bool exists = !r->overlay.list_children(key).empty();
  if (!r->static_is_overlay && !exists) exists = !r->static_store.list_children(key).empty();
  if (!exists && create) {
    r->overlay.write(key + "/.meta", "meta");
    return true;
  }
  return exists;
}

void VoxelSpacingImpl::remove() {
  root()->overlay.remove_prefix(path());
}
void TomogramImpl::remove() {
  root()->overlay.remove_prefix(zarr_path());
}
void FeaturesImpl::remove() {
  root()->overlay.remove_prefix(zarr_path());
}

// The KvStore to read an entity from: static for read-only entities in a two-source
// project, overlay otherwise. Writes always target the overlay.
io::KvStore& read_store(RootImpl* r, bool read_only) {
  return (read_only && !r->static_is_overlay) ? r->static_store : r->overlay;
}

void write_ome_array(io::KvStore& store, const std::string& zpath, const Array3D& data, double vs) {
  // NOTE: single pyramid level for now; multi-level pyramids are a follow-up.
  store.write(zpath + "/.zgroup", io::zgroup_json());
  store.write(zpath + "/.zattrs", io::ome_multiscales_json(vs, 1));
  io::zarr_write(store.kvstore_spec(zpath + "/0"), data);
}

Array3D read_ome_level(io::KvStore& store, const std::string& zpath, const Region& region,
                       int level) {
  return io::zarr_read(store.kvstore_spec(zpath + "/" + std::to_string(level)), region);
}

namespace {

std::shared_ptr<VoxelSpacingImpl> make_vs(const std::shared_ptr<RunImpl>& run, double size) {
  auto v = std::make_shared<VoxelSpacingImpl>();
  v->run = run;
  v->voxel_size = size;
  return v;
}
std::shared_ptr<TomogramImpl> make_tomo(const std::shared_ptr<VoxelSpacingImpl>& vs,
                                        const std::string& type, bool read_only) {
  auto t = std::make_shared<TomogramImpl>();
  t->vs = vs;
  t->tomo_type = type;
  t->read_only = read_only;
  return t;
}
std::shared_ptr<FeaturesImpl> make_feat(const std::shared_ptr<TomogramImpl>& tomo,
                                        const std::string& ft, bool read_only) {
  auto f = std::make_shared<FeaturesImpl>();
  f->tomo = tomo;
  f->feature_type = ft;
  f->read_only = read_only;
  return f;
}

}  // namespace
}  // namespace detail

// ================================ Run (voxel spacings) ===========================
Root Run::root() const {
  return Root(impl_->root);
}

std::vector<VoxelSpacing> Run::voxel_spacings() const {
  auto* r = impl_->root.get();
  std::map<std::string, double> found;  // dedup by formatted size
  auto scan = [&](const io::KvStore& store) {
    for (const auto& c : store.list_children(impl_->path())) {
      if (c.rfind("VoxelSpacing", 0) == 0) {
        try {
          double v = std::stod(c.substr(std::string("VoxelSpacing").size()));
          found[detail::format_voxel_size(v)] = v;
        } catch (...) {
        }
      }
    }
  };
  scan(r->overlay);
  if (!r->static_is_overlay) scan(r->static_store);

  std::vector<double> sizes;
  for (const auto& kv : found) sizes.push_back(kv.second);
  std::sort(sizes.begin(), sizes.end());

  std::vector<VoxelSpacing> out;
  out.reserve(sizes.size());
  for (double s : sizes) out.push_back(VoxelSpacing(detail::make_vs(impl_, s)));
  return out;
}

VoxelSpacing Run::get_voxel_spacing(double voxel_size) const {
  const std::string target = detail::format_voxel_size(voxel_size);
  for (const auto& vs : voxel_spacings()) {
    if (detail::format_voxel_size(vs.voxel_size()) == target) return vs;
  }
  return VoxelSpacing();
}

VoxelSpacing Run::new_voxel_spacing(double voxel_size, bool exist_ok) {
  VoxelSpacing existing = get_voxel_spacing(voxel_size);
  if (existing.valid()) {
    if (exist_ok) return existing;
    throw ValidationError("Voxel spacing " + detail::format_voxel_size(voxel_size) +
                          " already exists.");
  }
  auto v = detail::make_vs(impl_, voxel_size);
  v->ensure(true);
  return VoxelSpacing(v);
}

// ================================ VoxelSpacing ===================================
double VoxelSpacing::voxel_size() const {
  return impl_->voxel_size;
}
Run VoxelSpacing::run() const {
  return Run(impl_->run);
}

std::vector<Tomogram> VoxelSpacing::tomograms() const {
  auto* r = impl_->root();
  std::map<std::string, bool> types;  // tomo_type -> read_only (overlay wins -> writable)
  auto scan = [&](const io::KvStore& store, bool read_only) {
    for (const auto& c : store.list_children(impl_->path())) {
      if (c.ends_with(".zarr") && c.find("features") == std::string::npos) {
        types[c.substr(0, c.size() - 5)] = read_only;  // strip ".zarr"
      }
    }
  };
  if (!r->static_is_overlay) scan(r->static_store, true);
  scan(r->overlay, false);

  std::vector<Tomogram> out;
  out.reserve(types.size());
  for (const auto& kv : types)
    out.push_back(Tomogram(detail::make_tomo(impl_, kv.first, kv.second)));
  return out;
}

Tomogram VoxelSpacing::get_tomogram(const std::string& tomo_type) const {
  for (const auto& t : tomograms()) {
    if (t.tomo_type() == tomo_type) return t;
  }
  return Tomogram();
}

Tomogram VoxelSpacing::new_tomogram(const std::string& tomo_type, bool exist_ok) {
  Tomogram existing = get_tomogram(tomo_type);
  if (existing.valid()) {
    if (exist_ok) return existing;
    throw ValidationError("Tomogram type " + tomo_type + " already exists.");
  }
  return Tomogram(detail::make_tomo(impl_, tomo_type, false));
}

// ================================ Tomogram =======================================
const std::string& Tomogram::tomo_type() const {
  return impl_->tomo_type;
}
double Tomogram::voxel_size() const {
  return impl_->vs->voxel_size;
}
VoxelSpacing Tomogram::voxel_spacing() const {
  return VoxelSpacing(impl_->vs);
}

Array3D Tomogram::to_array(const Region& region, int level) const {
  auto& store = detail::read_store(impl_->root(), impl_->read_only);
  return detail::read_ome_level(store, impl_->zarr_path(), region, level);
}

void Tomogram::from_array(const Array3D& data, int levels) {
  (void)levels;  // single-level for now
  if (impl_->read_only) throw PermissionError("Cannot write to a read-only tomogram.");
  detail::write_ome_array(impl_->root()->overlay, impl_->zarr_path(), data, impl_->vs->voxel_size);
}

void Tomogram::set_region(const Array3D& data, const Region& region, int level) {
  if (impl_->read_only) throw PermissionError("Cannot write to a read-only tomogram.");
  io::zarr_write_region(
      impl_->root()->overlay.kvstore_spec(impl_->zarr_path() + "/" + std::to_string(level)), data,
      region);
}

std::vector<Features> Tomogram::features() const {
  auto* r = impl_->root();
  const std::string prefix = impl_->tomo_type + "_";
  const std::string suffix = "_features.zarr";
  std::map<std::string, bool> fts;  // feature_type -> read_only
  auto scan = [&](const io::KvStore& store, bool read_only) {
    for (const auto& c : store.list_children(impl_->vs->path())) {
      if (c.size() > prefix.size() + suffix.size() && c.starts_with(prefix) &&
          c.ends_with(suffix)) {
        fts[c.substr(prefix.size(), c.size() - prefix.size() - suffix.size())] = read_only;
      }
    }
  };
  if (!r->static_is_overlay) scan(r->static_store, true);
  scan(r->overlay, false);

  std::vector<Features> out;
  out.reserve(fts.size());
  for (const auto& kv : fts) out.push_back(Features(detail::make_feat(impl_, kv.first, kv.second)));
  return out;
}

Features Tomogram::get_features(const std::string& feature_type) const {
  for (const auto& f : features()) {
    if (f.feature_type() == feature_type) return f;
  }
  return Features();
}

Features Tomogram::new_features(const std::string& feature_type, bool exist_ok) {
  Features existing = get_features(feature_type);
  if (existing.valid()) {
    if (exist_ok) return existing;
    throw ValidationError("Feature type " + feature_type + " already exists.");
  }
  return Features(detail::make_feat(impl_, feature_type, false));
}

// ================================ Features =======================================
const std::string& Features::feature_type() const {
  return impl_->feature_type;
}
const std::string& Features::tomo_type() const {
  return impl_->tomo->tomo_type;
}
Tomogram Features::tomogram() const {
  return Tomogram(impl_->tomo);
}

Array3D Features::to_array(const Region& region, int level) const {
  auto& store = detail::read_store(impl_->root(), impl_->read_only);
  return detail::read_ome_level(store, impl_->zarr_path(), region, level);
}

void Features::from_array(const Array3D& data, int levels) {
  (void)levels;
  if (impl_->read_only) throw PermissionError("Cannot write to read-only features.");
  detail::write_ome_array(impl_->root()->overlay, impl_->zarr_path(), data,
                          impl_->tomo->vs->voxel_size);
}

void Features::set_region(const Array3D& data, const Region& region, int level) {
  if (impl_->read_only) throw PermissionError("Cannot write to read-only features.");
  io::zarr_write_region(
      impl_->root()->overlay.kvstore_spec(impl_->zarr_path() + "/" + std::to_string(level)), data,
      region);
}

}  // namespace copick
