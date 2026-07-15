#include "io/ome.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>

#include "copick/errors.h"

using json = ::nlohmann::json;

namespace copick {
namespace io {
namespace {

json ome_axes() {
  return json::array({
      json{{"name", "z"}, {"type", "space"}, {"unit", "angstrom"}},
      json{{"name", "y"}, {"type", "space"}, {"unit", "angstrom"}},
      json{{"name", "x"}, {"type", "space"}, {"unit", "angstrom"}},
  });
}

// Unit -> angstrom conversion factors (subset of copick.util.ome.UNITFACTOR; the common
// spatial units). Unknown units default to 1.0.
double unit_factor(const std::string& unit) {
  static const std::unordered_map<std::string, double> kFactors = {
      {"angstrom", 1.0},    {"nanometer", 1e1},  {"micrometer", 1e4}, {"millimeter", 1e7},
      {"picometer", 1e-2},  {"meter", 1e10},     {"centimeter", 1e8}, {"decimeter", 1e9},
      {"femtometer", 1e-5}, {"kilometer", 1e13},
  };
  const auto it = kFactors.find(unit);
  return it == kFactors.end() ? 1.0 : it->second;
}

}  // namespace

std::string zgroup_json() {
  return "{\"zarr_format\":2}";
}

std::string ome_multiscales_json(double voxel_size, int levels) {
  if (levels < 1) levels = 1;
  json datasets = json::array();
  double vs = voxel_size;
  for (int i = 0; i < levels; ++i) {
    datasets.push_back(json{
        {"path", std::to_string(i)},
        {"coordinateTransformations",
         json::array({json{{"type", "scale"}, {"scale", json::array({vs, vs, vs})}}})},
    });
    vs *= 2.0;
  }
  json multiscale = {{"version", "0.4"}, {"axes", ome_axes()}, {"datasets", datasets}};
  return json{{"multiscales", json::array({multiscale})}}.dump();
}

double voxel_size_from_zattrs(const std::string& zattrs_json) {
  json j;
  try {
    j = json::parse(zattrs_json);
  } catch (const std::exception& e) {
    throw ValidationError(std::string("Invalid OME .zattrs JSON: ") + e.what());
  }
  if (!j.contains("multiscales") || j["multiscales"].empty()) {
    throw ValidationError("OME .zattrs missing 'multiscales'.");
  }
  const json& ms = j["multiscales"][0];

  std::string unit = "angstrom";
  if (ms.contains("axes")) {
    for (const auto& ax : ms["axes"]) {
      if (ax.value("type", "") == "space" && ax.contains("unit")) {
        unit = ax["unit"].get<std::string>();
        break;
      }
    }
  }

  const json& ds0 = ms.at("datasets").at(0);
  for (const auto& t : ds0.at("coordinateTransformations")) {
    if (t.value("type", "") == "scale") {
      const double scale = t.at("scale").at(0).get<double>();
      return scale * unit_factor(unit);
    }
  }
  throw ValidationError("No scale transformation found in OME coordinate transformations.");
}

}  // namespace io
}  // namespace copick
