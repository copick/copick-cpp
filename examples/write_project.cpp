// Example: create a copick project with copick-cpp. Also used for the cross-implementation
// parity check (the same project is read back by Python copick).
//
//   write_project <absolute_project_dir>
//
// Writes <dir>/config.json and a project containing a run TS_001 with a 10.0 voxel-spacing
// tomogram, a segmentation, picks, and a mesh.
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "copick/copick.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: write_project <absolute_project_dir>\n";
    return 2;
  }
  const std::string dir = argv[1];
  const std::string config_path = dir + "/config.json";
  const std::string overlay_root = "local://" + dir + "/project";

  // Build a config with one pickable object and open the project.
  const std::string config_json =
      std::string(R"({"config_type":"filesystem","user_id":"cpp",)") +
      R"("pickable_objects":[{"name":"proteasome","is_particle":true,)" +
      R"("label":1,"color":[255,0,0,255]}],"overlay_root":")" + overlay_root + R"("})";
  copick::Root root = copick::from_string(config_json);
  root.save_config(config_path);

  copick::Run run = root.new_run("TS_001");

  // Tomogram: a 4x4x4 float32 ramp.
  copick::VoxelSpacing vs = run.new_voxel_spacing(10.0);
  copick::Tomogram tomo = vs.new_tomogram("wbp");
  copick::Array3D vol(copick::DType::Float32, 4, 4, 4);
  for (std::size_t i = 0; i < vol.size(); ++i) vol.data_as<float>()[i] = static_cast<float>(i);
  tomo.from_array(vol);

  // Segmentation: a 4x4x4 uint8 checkerboard.
  copick::Segmentation seg =
      run.new_segmentation(10.0, "proteasome", "1", /*multilabel=*/false, "cpp");
  copick::Array3D mask(copick::DType::UInt8, 4, 4, 4);
  for (std::size_t i = 0; i < mask.size(); ++i) mask.data_as<std::uint8_t>()[i] = i % 2;
  seg.from_array(mask);

  // Picks: two oriented points.
  copick::Picks picks = run.new_picks("proteasome", "0", "cpp");
  std::vector<copick::Point> pts;
  for (int i = 0; i < 2; ++i) {
    copick::Point p;
    p.location.x = 10.0 * i + 1;
    p.location.y = 10.0 * i + 2;
    p.location.z = 10.0 * i + 3;
    p.instance_id = i;
    pts.push_back(p);
  }
  picks.set_points(pts);
  picks.store();

  // Mesh: a tetrahedron.
  copick::Mesh mesh = run.new_mesh("proteasome", "0", "cpp");
  copick::Geometry geo;
  geo.vertices = {{{0.f, 0.f, 0.f}}, {{10.f, 0.f, 0.f}}, {{0.f, 10.f, 0.f}}, {{0.f, 0.f, 10.f}}};
  geo.faces = {{{0, 1, 2}}, {{0, 1, 3}}, {{0, 2, 3}}, {{1, 2, 3}}};
  mesh.set_mesh(geo);
  mesh.store();

  std::cout << "wrote project + config to " << dir << "\n";
  return 0;
}
