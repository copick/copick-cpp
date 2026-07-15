#include "io/zarr.hpp"

#include <tensorstore/array.h>
#include <tensorstore/context.h>
#include <tensorstore/data_type.h>
#include <tensorstore/index_space/dim_expression.h>
#include <tensorstore/open.h>
#include <tensorstore/open_mode.h>
#include <tensorstore/tensorstore.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstring>
#include <utility>

#include "copick/errors.h"
#include "io/context.hpp"

namespace ts = tensorstore;
using json = ::nlohmann::json;

namespace copick {
namespace io {
namespace {

// --- dtype mapping ---------------------------------------------------------------
template <class T>
struct dtype_of;
#define COPICK_DTYPE_TRAIT(T, D)             \
  template <>                                \
  struct dtype_of<T> {                       \
    static constexpr DType value = DType::D; \
  }
COPICK_DTYPE_TRAIT(float, Float32);
COPICK_DTYPE_TRAIT(double, Float64);
COPICK_DTYPE_TRAIT(std::int8_t, Int8);
COPICK_DTYPE_TRAIT(std::uint8_t, UInt8);
COPICK_DTYPE_TRAIT(std::int16_t, Int16);
COPICK_DTYPE_TRAIT(std::uint16_t, UInt16);
COPICK_DTYPE_TRAIT(std::int32_t, Int32);
COPICK_DTYPE_TRAIT(std::uint32_t, UInt32);
COPICK_DTYPE_TRAIT(std::int64_t, Int64);
COPICK_DTYPE_TRAIT(std::uint64_t, UInt64);
#undef COPICK_DTYPE_TRAIT

const char* zarr_dtype_string(DType dt) {
  switch (dt) {
    case DType::Float32:
      return "<f4";
    case DType::Float64:
      return "<f8";
    case DType::Int8:
      return "|i1";
    case DType::UInt8:
      return "|u1";
    case DType::Int16:
      return "<i2";
    case DType::UInt16:
      return "<u2";
    case DType::Int32:
      return "<i4";
    case DType::UInt32:
      return "<u4";
    case DType::Int64:
      return "<i8";
    case DType::UInt64:
      return "<u8";
  }
  throw Error("zarr: unhandled dtype");
}

DType dtype_from_ts(ts::DataType dt) {
  if (dt == ts::dtype_v<float>) return DType::Float32;
  if (dt == ts::dtype_v<double>) return DType::Float64;
  if (dt == ts::dtype_v<std::int8_t>) return DType::Int8;
  if (dt == ts::dtype_v<std::uint8_t>) return DType::UInt8;
  if (dt == ts::dtype_v<std::int16_t>) return DType::Int16;
  if (dt == ts::dtype_v<std::uint16_t>) return DType::UInt16;
  if (dt == ts::dtype_v<std::int32_t>) return DType::Int32;
  if (dt == ts::dtype_v<std::uint32_t>) return DType::UInt32;
  if (dt == ts::dtype_v<std::int64_t>) return DType::Int64;
  if (dt == ts::dtype_v<std::uint64_t>) return DType::UInt64;
  throw Error("zarr: unsupported array dtype");
}

// --- helpers ---------------------------------------------------------------------
std::pair<ts::Index, ts::Index> resolve_range(const Range& r, ts::Index extent) {
  const ts::Index start = static_cast<ts::Index>(r.start);
  const ts::Index stop = (r.stop == Range::kEnd) ? extent : static_cast<ts::Index>(r.stop);
  if (start < 0 || stop > extent || start > stop) throw Error("zarr: region out of bounds");
  return {start, stop};
}

json zarr_spec(const json& kvstore) {
  return json{{"driver", "zarr"}, {"kvstore", kvstore}};
}

// Build a zarr v2 compressor spec. blosc self-describes its uncompressed size and is
// universally readable (unlike tensorstore's raw zstd frames — see zarr.hpp).
json compressor_spec(const std::string& name, int level) {
  if (name.empty() || name == "none") return json(nullptr);
  if (name == "gzip") return json{{"id", "gzip"}, {"level", level}};
  if (name == "zstd") return json{{"id", "zstd"}, {"level", level}};
  if (name == "blosc") {
    return json{{"id", "blosc"}, {"cname", "zstd"}, {"clevel", level}, {"shuffle", -1}};
  }
  throw Error("zarr: unknown compressor '" + name + "'");
}

template <class T>
Array3D read_typed(const json& spec, const Region& region) {
  auto store =
      ts::Open<T, 3>(spec, io::shared_context(), ts::OpenMode::open, ts::ReadWriteMode::read)
          .result();
  if (!store.ok()) throw Error("zarr open (read) failed: " + store.status().ToString());
  const auto shape = store->domain().shape();
  const auto z = resolve_range(region.z, shape[0]);
  const auto y = resolve_range(region.y, shape[1]);
  const auto x = resolve_range(region.x, shape[2]);

  auto arr = ts::Read<ts::zero_origin>(*store | ts::Dims(0).HalfOpenInterval(z.first, z.second) |
                                       ts::Dims(1).HalfOpenInterval(y.first, y.second) |
                                       ts::Dims(2).HalfOpenInterval(x.first, x.second))
                 .result();
  if (!arr.ok()) throw Error("zarr read failed: " + arr.status().ToString());

  Array3D out(dtype_of<T>::value, static_cast<std::size_t>(z.second - z.first),
              static_cast<std::size_t>(y.second - y.first),
              static_cast<std::size_t>(x.second - x.first));
  std::memcpy(out.bytes(), arr->data(), out.nbytes());
  return out;
}

template <class T>
void write_typed(const json& kvstore, const Array3D& data, const ZarrWriteOptions& opts) {
  const std::size_t cz = opts.chunk_z ? opts.chunk_z : data.shape_z();
  const std::size_t cy = opts.chunk_y ? opts.chunk_y : data.shape_y();
  const std::size_t cx = opts.chunk_x ? opts.chunk_x : data.shape_x();

  json meta = {
      {"dtype", zarr_dtype_string(dtype_of<T>::value)},
      {"shape", {data.shape_z(), data.shape_y(), data.shape_x()}},
      {"chunks", {cz, cy, cx}},
      {"dimension_separator", "/"},
      {"compressor", compressor_spec(opts.compressor, opts.compression_level)},
  };

  json spec = zarr_spec(kvstore);
  spec["metadata"] = std::move(meta);

  auto store = ts::Open<T, 3>(spec, io::shared_context(),
                              ts::OpenMode::create | ts::OpenMode::delete_existing,
                              ts::ReadWriteMode::read_write)
                   .result();
  if (!store.ok()) throw Error("zarr open (create) failed: " + store.status().ToString());

  auto arr = ts::AllocateArray<T>({static_cast<ts::Index>(data.shape_z()),
                                   static_cast<ts::Index>(data.shape_y()),
                                   static_cast<ts::Index>(data.shape_x())});
  std::memcpy(arr.data(), data.bytes(), data.nbytes());

  auto w = ts::Write(arr, *store).result();
  if (!w.ok()) throw Error("zarr write failed: " + w.status().ToString());
}

template <class T>
void write_region_typed(const json& kvstore, const Array3D& data, const Region& region) {
  auto store = ts::Open<T, 3>(zarr_spec(kvstore), io::shared_context(), ts::OpenMode::open,
                              ts::ReadWriteMode::read_write)
                   .result();
  if (!store.ok()) throw Error("zarr open (region write) failed: " + store.status().ToString());
  const auto shape = store->domain().shape();
  const auto z = resolve_range(region.z, shape[0]);
  const auto y = resolve_range(region.y, shape[1]);
  const auto x = resolve_range(region.x, shape[2]);

  if (data.shape_z() != static_cast<std::size_t>(z.second - z.first) ||
      data.shape_y() != static_cast<std::size_t>(y.second - y.first) ||
      data.shape_x() != static_cast<std::size_t>(x.second - x.first)) {
    throw Error("zarr: data shape does not match region extent");
  }

  auto arr = ts::AllocateArray<T>({static_cast<ts::Index>(data.shape_z()),
                                   static_cast<ts::Index>(data.shape_y()),
                                   static_cast<ts::Index>(data.shape_x())});
  std::memcpy(arr.data(), data.bytes(), data.nbytes());

  auto w = ts::Write(arr, *store | ts::Dims(0).HalfOpenInterval(z.first, z.second) |
                              ts::Dims(1).HalfOpenInterval(y.first, y.second) |
                              ts::Dims(2).HalfOpenInterval(x.first, x.second))
               .result();
  if (!w.ok()) throw Error("zarr region write failed: " + w.status().ToString());
}

#define COPICK_DISPATCH_DTYPE(dt, FN, ...)     \
  do {                                         \
    switch (dt) {                              \
      case DType::Float32:                     \
        return FN<float>(__VA_ARGS__);         \
      case DType::Float64:                     \
        return FN<double>(__VA_ARGS__);        \
      case DType::Int8:                        \
        return FN<std::int8_t>(__VA_ARGS__);   \
      case DType::UInt8:                       \
        return FN<std::uint8_t>(__VA_ARGS__);  \
      case DType::Int16:                       \
        return FN<std::int16_t>(__VA_ARGS__);  \
      case DType::UInt16:                      \
        return FN<std::uint16_t>(__VA_ARGS__); \
      case DType::Int32:                       \
        return FN<std::int32_t>(__VA_ARGS__);  \
      case DType::UInt32:                      \
        return FN<std::uint32_t>(__VA_ARGS__); \
      case DType::Int64:                       \
        return FN<std::int64_t>(__VA_ARGS__);  \
      case DType::UInt64:                      \
        return FN<std::uint64_t>(__VA_ARGS__); \
    }                                          \
    throw Error("zarr: unhandled dtype");      \
  } while (0)

}  // namespace

ZarrInfo zarr_info(const std::string& kvstore_spec_json) {
  auto store = ts::Open(zarr_spec(json::parse(kvstore_spec_json)), io::shared_context(),
                        ts::OpenMode::open, ts::ReadWriteMode::read)
                   .result();
  if (!store.ok()) throw Error("zarr open (info) failed: " + store.status().ToString());
  if (store->rank() != 3) throw Error("zarr: expected a rank-3 array");
  const auto shape = store->domain().shape();
  return ZarrInfo{static_cast<std::size_t>(shape[0]), static_cast<std::size_t>(shape[1]),
                  static_cast<std::size_t>(shape[2]), dtype_from_ts(store->dtype())};
}

Array3D zarr_read(const std::string& kvstore_spec_json, const Region& region) {
  const json kvstore = json::parse(kvstore_spec_json);
  const json spec = zarr_spec(kvstore);
  const ZarrInfo info = zarr_info(kvstore_spec_json);
  COPICK_DISPATCH_DTYPE(info.dtype, read_typed, spec, region);
}

void zarr_write(const std::string& kvstore_spec_json, const Array3D& data,
                const ZarrWriteOptions& opts) {
  const json kvstore = json::parse(kvstore_spec_json);
  COPICK_DISPATCH_DTYPE(data.dtype(), write_typed, kvstore, data, opts);
}

void zarr_write_region(const std::string& kvstore_spec_json, const Array3D& data,
                       const Region& region) {
  const json kvstore = json::parse(kvstore_spec_json);
  COPICK_DISPATCH_DTYPE(data.dtype(), write_region_typed, kvstore, data, region);
}

}  // namespace io
}  // namespace copick
