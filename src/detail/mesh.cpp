// Mesh entity: GLB load/store via tinygltf, plus the Run mesh accessors. Compiled at
// C++20. The tinygltf-backed GLB (de)serialization is gated on COPICK_ENABLE_MESH; when
// that is off, load/store throw and only metadata/enumeration work.
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "copick/errors.h"
#include "copick/escape.h"
#include "copick/mesh.h"
#include "copick/run.h"
#include "detail/impl.hpp"
#include "io/kvstore.hpp"

#ifdef COPICK_ENABLE_MESH
#include <cstdint>
#include <cstring>
#include <sstream>
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include <tiny_gltf.h>
#endif

namespace copick {
namespace detail {

#ifdef COPICK_ENABLE_MESH
namespace {

std::string geometry_to_glb(const Geometry& g) {
  tinygltf::Model model;
  const std::size_t vbytes = g.vertices.size() * 3 * sizeof(float);
  const std::size_t ibytes = g.faces.size() * 3 * sizeof(std::uint32_t);

  tinygltf::Buffer buffer;
  buffer.data.resize(vbytes + ibytes);
  if (vbytes) std::memcpy(buffer.data.data(), g.vertices.data(), vbytes);
  if (ibytes) std::memcpy(buffer.data.data() + vbytes, g.faces.data(), ibytes);
  model.buffers.push_back(std::move(buffer));

  tinygltf::BufferView vbv;
  vbv.buffer = 0;
  vbv.byteOffset = 0;
  vbv.byteLength = vbytes;
  vbv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
  tinygltf::BufferView ibv;
  ibv.buffer = 0;
  ibv.byteOffset = vbytes;
  ibv.byteLength = ibytes;
  ibv.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
  model.bufferViews.push_back(vbv);
  model.bufferViews.push_back(ibv);

  tinygltf::Accessor pos;
  pos.bufferView = 0;
  pos.byteOffset = 0;
  pos.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
  pos.count = g.vertices.size();
  pos.type = TINYGLTF_TYPE_VEC3;
  float mn[3] = {0, 0, 0}, mx[3] = {0, 0, 0};
  if (!g.vertices.empty()) {
    for (int k = 0; k < 3; ++k) mn[k] = mx[k] = g.vertices[0][k];
    for (const auto& v : g.vertices)
      for (int k = 0; k < 3; ++k) {
        mn[k] = std::min(mn[k], v[k]);
        mx[k] = std::max(mx[k], v[k]);
      }
  }
  pos.minValues = {mn[0], mn[1], mn[2]};
  pos.maxValues = {mx[0], mx[1], mx[2]};
  model.accessors.push_back(pos);

  tinygltf::Accessor idx;
  idx.bufferView = 1;
  idx.byteOffset = 0;
  idx.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
  idx.count = g.faces.size() * 3;
  idx.type = TINYGLTF_TYPE_SCALAR;
  model.accessors.push_back(idx);

  tinygltf::Primitive prim;
  prim.attributes["POSITION"] = 0;
  prim.indices = 1;
  prim.mode = TINYGLTF_MODE_TRIANGLES;
  tinygltf::Mesh mesh;
  mesh.primitives.push_back(prim);
  model.meshes.push_back(mesh);

  tinygltf::Node node;
  node.mesh = 0;
  model.nodes.push_back(node);
  tinygltf::Scene scene;
  scene.nodes.push_back(0);
  model.scenes.push_back(scene);
  model.defaultScene = 0;
  model.asset.version = "2.0";
  model.asset.generator = "copick-cpp";

  std::ostringstream oss;
  tinygltf::TinyGLTF ctx;
  if (!ctx.WriteGltfSceneToStream(&model, oss, /*prettyPrint=*/false, /*writeBinary=*/true)) {
    throw Error("Failed to serialize mesh to GLB.");
  }
  return oss.str();
}

Geometry glb_to_geometry(const std::string& bytes) {
  tinygltf::Model model;
  tinygltf::TinyGLTF ctx;
  std::string err, warn;
  const bool ok = ctx.LoadBinaryFromMemory(
      &model, &err, &warn, reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
  if (!ok) throw Error("Failed to parse GLB: " + err);

  Geometry g;
  if (model.meshes.empty() || model.meshes[0].primitives.empty()) return g;
  const auto& prim = model.meshes[0].primitives[0];

  const auto pit = prim.attributes.find("POSITION");
  if (pit != prim.attributes.end()) {
    const auto& acc = model.accessors[pit->second];
    const auto& bv = model.bufferViews[acc.bufferView];
    const unsigned char* base =
        model.buffers[bv.buffer].data.data() + bv.byteOffset + acc.byteOffset;
    const std::size_t stride = bv.byteStride ? bv.byteStride : 3 * sizeof(float);
    g.vertices.resize(acc.count);
    for (std::size_t i = 0; i < acc.count; ++i) {
      const float* f = reinterpret_cast<const float*>(base + i * stride);
      g.vertices[i] = {f[0], f[1], f[2]};
    }
  }

  if (prim.indices >= 0) {
    const auto& acc = model.accessors[prim.indices];
    const auto& bv = model.bufferViews[acc.bufferView];
    const unsigned char* base =
        model.buffers[bv.buffer].data.data() + bv.byteOffset + acc.byteOffset;
    const std::size_t n = acc.count;
    std::vector<std::uint32_t> idx(n);
    switch (acc.componentType) {
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        for (std::size_t i = 0; i < n; ++i)
          idx[i] = reinterpret_cast<const std::uint32_t*>(base)[i];
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        for (std::size_t i = 0; i < n; ++i)
          idx[i] = reinterpret_cast<const std::uint16_t*>(base)[i];
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        for (std::size_t i = 0; i < n; ++i) idx[i] = base[i];
        break;
      default:
        throw Error("Unsupported GLB index component type.");
    }
    g.faces.resize(n / 3);
    for (std::size_t i = 0; i < n / 3; ++i)
      g.faces[i] = {idx[3 * i], idx[3 * i + 1], idx[3 * i + 2]};
  }
  return g;
}

}  // namespace
#endif  // COPICK_ENABLE_MESH

void MeshImpl::load() {
#ifdef COPICK_ENABLE_MESH
  auto& store = read_store(root(), read_only);
  const auto data = store.read(path());
  if (!data) throw NotFoundError("Mesh file not found: " + path());
  geom = glb_to_geometry(*data);
  loaded = true;
#else
  throw Error("Mesh support not compiled in (build with COPICK_ENABLE_MESH).");
#endif
}

void MeshImpl::store() {
#ifdef COPICK_ENABLE_MESH
  if (read_only) throw PermissionError("Cannot store a read-only mesh.");
  root()->overlay.write(path(), geometry_to_glb(geom));
#else
  throw Error("Mesh support not compiled in (build with COPICK_ENABLE_MESH).");
#endif
}

void MeshImpl::remove() {
  root()->overlay.remove(path());
}

namespace {
std::shared_ptr<MeshImpl> make_mesh(const std::shared_ptr<RunImpl>& run, const std::string& obj,
                                    const std::string& user, const std::string& session,
                                    bool read_only) {
  auto m = std::make_shared<MeshImpl>();
  m->run = run;
  m->object_name = obj;
  m->user_id = user;
  m->session_id = session;
  m->read_only = read_only;
  return m;
}
}  // namespace

}  // namespace detail

// ================================ Mesh handle ====================================
const std::string& Mesh::object_name() const {
  return impl_->object_name;
}
const std::string& Mesh::user_id() const {
  return impl_->user_id;
}
const std::string& Mesh::session_id() const {
  return impl_->session_id;
}
bool Mesh::from_tool() const {
  return impl_->session_id == "0";
}
bool Mesh::from_user() const {
  return impl_->session_id != "0";
}
Run Mesh::run() const {
  return Run(impl_->run);
}

const Geometry& Mesh::mesh() const {
  if (!impl_->loaded) impl_->load();
  return impl_->geom;
}
void Mesh::set_mesh(Geometry geometry) const {
  impl_->geom = std::move(geometry);
  impl_->loaded = true;
}
void Mesh::store() const {
  impl_->store();
}
void Mesh::refresh() const {
  impl_->load();
}

// ================================ Run (meshes) ===================================
std::vector<Mesh> Run::meshes() const {
  auto* r = impl_->root.get();
  std::map<std::string, Mesh> found;
  auto scan = [&](const io::KvStore& store, bool read_only) {
    for (const auto& c : store.list_children(impl_->path() + "/Meshes")) {
      if (c.empty() || c[0] == '.' || !c.ends_with(".glb")) continue;
      const auto parts = detail::split_on(c.substr(0, c.size() - 4), '_');
      if (parts.size() < 3) continue;
      const std::string key = parts[0] + "_" + parts[1] + "_" + parts[2];
      found[key] = Mesh(detail::make_mesh(impl_, parts[2], parts[0], parts[1], read_only));
    }
  };
  if (!r->static_is_overlay) scan(r->static_store, true);
  scan(r->overlay, false);

  std::vector<Mesh> out;
  out.reserve(found.size());
  for (auto& kv : found) out.push_back(kv.second);
  return out;
}

std::vector<Mesh> Run::get_meshes(const std::string& object_name, const std::string& user_id,
                                  const std::string& session_id) const {
  std::vector<Mesh> out;
  for (auto& m : meshes()) {
    if (!object_name.empty() && m.object_name() != object_name) continue;
    if (!user_id.empty() && m.user_id() != user_id) continue;
    if (!session_id.empty() && m.session_id() != session_id) continue;
    out.push_back(m);
  }
  return out;
}

Mesh Run::new_mesh(const std::string& object_name, const std::string& session_id,
                   const std::string& user_id, bool exist_ok) {
  auto* r = impl_->root.get();
  const std::string obj = copick::sanitize_name(object_name);
  const std::string sess = copick::sanitize_name(session_id);
  const std::string uid =
      detail::resolve_user(r, user_id.empty() ? "" : copick::sanitize_name(user_id), "new_mesh");

  if (!detail::is_pickable(r, obj)) {
    throw ValidationError("Object name " + obj + " not found in pickable objects.");
  }

  auto existing = get_meshes(obj, uid, sess);
  if (!existing.empty()) {
    if (exist_ok) return existing[0];
    throw ValidationError("Mesh for " + obj + " by " + uid + " already exists in session " + sess +
                          ".");
  }
  auto mi = detail::make_mesh(impl_, obj, uid, sess, false);
  mi->loaded = true;  // brand new -> empty geometry
  mi->store();        // write an (empty) GLB so it enumerates
  return Mesh(mi);
}

}  // namespace copick
