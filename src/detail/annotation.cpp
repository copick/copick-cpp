// Annotation entities: Object (density maps), Picks (JSON), Segmentation (zarr), plus
// the Run/Root accessors for them. Compiled at C++20, only under COPICK_ENABLE_ZARR.
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "copick/errors.h"
#include "copick/escape.h"
#include "copick/object.h"
#include "copick/picks.h"
#include "copick/root.h"
#include "copick/run.h"
#include "copick/segmentation.h"
#include "copick/serialize.h"
#include "detail/impl.hpp"
#include "io/kvstore.hpp"
#include "io/zarr.hpp"

namespace copick {
namespace detail {

std::vector<std::string> split_on(const std::string& s, char delim) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == delim) {
      out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  out.push_back(cur);
  return out;
}

bool is_pickable(RootImpl* r, const std::string& name) {
  for (const auto& o : r->config.pickable_objects) {
    if (o.name == name) return true;
  }
  return false;
}

// Resolve/validate the effective user id (supplied, else config user_id).
std::string resolve_user(RootImpl* r, const std::string& user_id, const char* fn) {
  std::string uid =
      user_id.empty() ? (r->config.user_id ? *r->config.user_id : std::string()) : user_id;
  if (uid.empty()) {
    throw ValidationError(std::string("User ID must be set in the config or supplied to ") + fn +
                          ".");
  }
  return uid;
}

// --- impl methods ----------------------------------------------------------------
void PicksImpl::load() {
  auto& store = read_store(root(), read_only);
  const auto data = store.read(path());
  if (!data) throw NotFoundError("Picks file not found: " + path());
  file = picks_from_json(*data);
  loaded = true;
}

void PicksImpl::store() {
  if (read_only) throw PermissionError("Cannot store picks in a read-only source.");
  root()->overlay.write(path(), picks_to_json(file));
}

void PicksImpl::remove() {
  root()->overlay.remove(path());
}

std::string SegmentationImpl::filename() const {
  std::string base = format_voxel_size(voxel_size) + "_" + user_id + "_" + session_id + "_" + name;
  return base + (is_multilabel ? "-multilabel.zarr" : ".zarr");
}

void SegmentationImpl::remove() {
  root()->overlay.remove_prefix(path());
}

namespace {

std::shared_ptr<PicksImpl> make_picks(const std::shared_ptr<RunImpl>& run, const std::string& obj,
                                      const std::string& user, const std::string& session,
                                      bool read_only) {
  auto p = std::make_shared<PicksImpl>();
  p->run = run;
  p->file.pickable_object_name = obj;
  p->file.user_id = user;
  p->file.session_id = session;
  p->file.run_name = run->name;
  p->read_only = read_only;
  return p;
}

std::shared_ptr<SegmentationImpl> make_seg(const std::shared_ptr<RunImpl>& run, double vs,
                                           const std::string& user, const std::string& session,
                                           const std::string& name, bool multilabel,
                                           bool read_only) {
  auto s = std::make_shared<SegmentationImpl>();
  s->run = run;
  s->voxel_size = vs;
  s->user_id = user;
  s->session_id = session;
  s->name = name;
  s->is_multilabel = multilabel;
  s->read_only = read_only;
  return s;
}

}  // namespace
}  // namespace detail

// ================================ Object =========================================
const std::string& Object::name() const {
  return impl_->meta.name;
}
bool Object::is_particle() const {
  return impl_->meta.is_particle;
}
int Object::label() const {
  return impl_->meta.label;
}
const Color& Object::color() const {
  return impl_->meta.color;
}
const PickableObject& Object::meta() const {
  return impl_->meta;
}

bool Object::has_density() const {
  if (!impl_->meta.is_particle) return false;
  auto& store = detail::read_store(impl_->root.get(), impl_->read_only);
  return !store.list_children(impl_->zarr_path()).empty();
}

Array3D Object::numpy(const Region& region, int level) const {
  if (!impl_->meta.is_particle)
    throw ValidationError("Object " + impl_->meta.name + " is not a particle.");
  auto& store = detail::read_store(impl_->root.get(), impl_->read_only);
  return detail::read_ome_level(store, impl_->zarr_path(), region, level);
}

void Object::from_numpy(const Array3D& data, double voxel_size, int levels) {
  (void)levels;
  if (!impl_->meta.is_particle)
    throw ValidationError("Object " + impl_->meta.name + " is not a particle.");
  if (impl_->read_only) throw PermissionError("Cannot write to a read-only object.");
  detail::write_ome_array(impl_->root->overlay, impl_->zarr_path(), data, voxel_size);
}

// ================================ Root (objects) =================================
std::vector<Object> Root::pickable_objects() const {
  std::vector<Object> out;
  out.reserve(impl_->config.pickable_objects.size());
  for (const auto& meta : impl_->config.pickable_objects) {
    auto oi = std::make_shared<detail::ObjectImpl>();
    oi->root = impl_;
    oi->meta = meta;
    out.push_back(Object(oi));
  }
  return out;
}

Object Root::get_object(const std::string& name) const {
  for (const auto& meta : impl_->config.pickable_objects) {
    if (meta.name == name) {
      auto oi = std::make_shared<detail::ObjectImpl>();
      oi->root = impl_;
      oi->meta = meta;
      return Object(oi);
    }
  }
  return Object();
}

// ================================ Picks ==========================================
const std::string& Picks::object_name() const {
  return impl_->file.pickable_object_name;
}
const std::string& Picks::user_id() const {
  return impl_->file.user_id;
}
const std::string& Picks::session_id() const {
  return impl_->file.session_id;
}
bool Picks::from_tool() const {
  return impl_->file.session_id == "0";
}
bool Picks::from_user() const {
  return impl_->file.session_id != "0";
}
Run Picks::run() const {
  return Run(impl_->run);
}

const std::vector<Point>& Picks::points() const {
  if (!impl_->loaded) impl_->load();
  return impl_->file.points;
}
void Picks::set_points(std::vector<Point> points) const {
  impl_->file.points = std::move(points);
  impl_->loaded = true;
}
void Picks::store() const {
  impl_->store();
}
void Picks::refresh() const {
  impl_->load();
}

std::vector<Picks> Run::picks() const {
  auto* r = impl_->root.get();
  std::map<std::string, Picks> found;  // key -> handle (sorted, overlay wins)
  auto scan = [&](const io::KvStore& store, bool read_only) {
    for (const auto& c : store.list_children(impl_->path() + "/Picks")) {
      if (c.empty() || c[0] == '.' || !c.ends_with(".json")) continue;
      const auto parts = detail::split_on(c.substr(0, c.size() - 5), '_');
      if (parts.size() < 3) continue;
      const std::string key = parts[0] + "_" + parts[1] + "_" + parts[2];
      found[key] = Picks(detail::make_picks(impl_, parts[2], parts[0], parts[1], read_only));
    }
  };
  if (!r->static_is_overlay) scan(r->static_store, true);
  scan(r->overlay, false);  // overlay wins the key

  std::vector<Picks> out;
  out.reserve(found.size());
  for (auto& kv : found) out.push_back(kv.second);
  return out;
}

std::vector<Picks> Run::get_picks(const std::string& object_name, const std::string& user_id,
                                  const std::string& session_id) const {
  std::vector<Picks> out;
  for (auto& p : picks()) {
    if (!object_name.empty() && p.object_name() != object_name) continue;
    if (!user_id.empty() && p.user_id() != user_id) continue;
    if (!session_id.empty() && p.session_id() != session_id) continue;
    out.push_back(p);
  }
  return out;
}

Picks Run::new_picks(const std::string& object_name, const std::string& session_id,
                     const std::string& user_id, bool exist_ok) {
  auto* r = impl_->root.get();
  const std::string obj = sanitize_name(object_name);
  const std::string sess = sanitize_name(session_id);
  const std::string uid =
      detail::resolve_user(r, user_id.empty() ? "" : sanitize_name(user_id), "new_picks");

  if (!detail::is_pickable(r, obj)) {
    throw ValidationError("Object name " + obj + " not found in pickable objects.");
  }

  auto existing = get_picks(obj, uid, sess);
  if (!existing.empty()) {
    if (exist_ok) return existing[0];
    throw ValidationError("Picks for " + obj + " by " + uid + " already exist in session " + sess +
                          ".");
  }
  auto pi = detail::make_picks(impl_, obj, uid, sess, false);
  pi->loaded = true;  // brand new -> empty points, nothing to load
  pi->store();        // write the (empty) picks file
  return Picks(pi);
}

// ================================ Segmentation ===================================
const std::string& Segmentation::name() const {
  return impl_->name;
}
const std::string& Segmentation::user_id() const {
  return impl_->user_id;
}
const std::string& Segmentation::session_id() const {
  return impl_->session_id;
}
bool Segmentation::is_multilabel() const {
  return impl_->is_multilabel;
}
double Segmentation::voxel_size() const {
  return impl_->voxel_size;
}
bool Segmentation::from_tool() const {
  return impl_->session_id == "0";
}
bool Segmentation::from_user() const {
  return impl_->session_id != "0";
}
Run Segmentation::run() const {
  return Run(impl_->run);
}

Array3D Segmentation::numpy(const Region& region, int level) const {
  auto& store = detail::read_store(impl_->root(), impl_->read_only);
  return detail::read_ome_level(store, impl_->path(), region, level);
}
void Segmentation::from_numpy(const Array3D& data, int levels) {
  (void)levels;
  if (impl_->read_only) throw PermissionError("Cannot write to a read-only segmentation.");
  detail::write_ome_array(impl_->root()->overlay, impl_->path(), data, impl_->voxel_size);
}
void Segmentation::set_region(const Array3D& data, const Region& region, int level) {
  if (impl_->read_only) throw PermissionError("Cannot write to a read-only segmentation.");
  io::zarr_write_region(
      impl_->root()->overlay.kvstore_spec(impl_->path() + "/" + std::to_string(level)), data,
      region);
}

namespace detail {
namespace {
// Parse a segmentation filename "<vs>_<user>_<session>_<name>[-multilabel].zarr".
bool parse_seg(const std::string& c, double& vs, std::string& user, std::string& session,
               std::string& name, bool& multilabel) {
  if (!c.ends_with(".zarr") || c.empty() || c[0] == '.') return false;
  const auto parts = split_on(c.substr(0, c.size() - 5), '_');
  if (parts.size() < 4) return false;
  try {
    vs = std::stod(parts[0]);
  } catch (...) {
    return false;
  }
  user = parts[1];
  session = parts[2];
  name = parts[3];
  multilabel = false;
  static const std::string suffix = "-multilabel";
  if (name.size() > suffix.size() &&
      name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
    multilabel = true;
    name = name.substr(0, name.size() - suffix.size());
  }
  return true;
}
}  // namespace
}  // namespace detail

std::vector<Segmentation> Run::segmentations() const {
  auto* r = impl_->root.get();
  std::map<std::string, Segmentation> found;
  auto scan = [&](const io::KvStore& store, bool read_only) {
    for (const auto& c : store.list_children(impl_->path() + "/Segmentations")) {
      double vs;
      std::string user, session, name;
      bool ml;
      if (!detail::parse_seg(c, vs, user, session, name, ml)) continue;
      found[c] = Segmentation(detail::make_seg(impl_, vs, user, session, name, ml, read_only));
    }
  };
  if (!r->static_is_overlay) scan(r->static_store, true);
  scan(r->overlay, false);

  std::vector<Segmentation> out;
  out.reserve(found.size());
  for (auto& kv : found) out.push_back(kv.second);
  return out;
}

std::vector<Segmentation> Run::get_segmentations(const std::string& user_id,
                                                 const std::string& session_id,
                                                 const std::string& name) const {
  std::vector<Segmentation> out;
  for (auto& s : segmentations()) {
    if (!user_id.empty() && s.user_id() != user_id) continue;
    if (!session_id.empty() && s.session_id() != session_id) continue;
    if (!name.empty() && s.name() != name) continue;
    out.push_back(s);
  }
  return out;
}

Segmentation Run::new_segmentation(double voxel_size, const std::string& name,
                                   const std::string& session_id, bool is_multilabel,
                                   const std::string& user_id, bool exist_ok) {
  auto* r = impl_->root.get();
  const std::string nm = sanitize_name(name);
  const std::string sess = sanitize_name(session_id);
  const std::string uid =
      detail::resolve_user(r, user_id.empty() ? "" : sanitize_name(user_id), "new_segmentation");

  if (!is_multilabel && !detail::is_pickable(r, nm)) {
    throw ValidationError("Object name " + nm + " not found in pickable objects.");
  }

  for (auto& s : segmentations()) {
    if (s.user_id() == uid && s.session_id() == sess && s.name() == nm &&
        s.is_multilabel() == is_multilabel &&
        detail::format_voxel_size(s.voxel_size()) == detail::format_voxel_size(voxel_size)) {
      if (exist_ok) return s;
      throw ValidationError("Segmentation by " + uid + " already exists in session " + sess +
                            " with name " + nm + ".");
    }
  }
  return Segmentation(detail::make_seg(impl_, voxel_size, uid, sess, nm, is_multilabel, false));
}

}  // namespace copick
