# copick-cpp

A C++ port of [copick](https://github.com/copick/copick) — a storage-agnostic, server-less
cryo-ET dataset API. This port implements the **core data model** and the **filesystem storage
backend** (local; S3 planned), so C++ tools (e.g. AreTomo3) can read and write copick projects
natively. Projects written by copick-cpp are byte-compatible with the reference Python copick
(verified against copick 1.26.1 — see [Cross-implementation parity](#cross-implementation-parity)).

## Design

The **public headers** (`include/copick/`) are strictly **C++11** so they can be `#include`d from
older toolchains such as AreTomo3's `g++ -std=c++11` build. All heavy dependencies live behind a
pImpl firewall in `src/` (compiled at C++20) and are linked **PRIVATE**, so none leak to consumers:

| Concern | Library | Standard | Where |
|---|---|---|---|
| JSON (de)serialization + validation | [reflect-cpp](https://github.com/getml/reflect-cpp) | C++20 | `src/` only |
| Zarr v2 arrays + `file`/`s3` storage | [tensorstore](https://github.com/google/tensorstore) | C++17 | `src/` only (opt-in) |
| GLB mesh I/O | [tinygltf](https://github.com/syoyo/tinygltf) | C++11 | `src/` only (opt-in) |

## Requirements

- CMake ≥ 3.24 and Ninja.
- A C++20 compiler for the library (GCC ≥ 11 or Clang ≥ 14, for reflect-cpp).
- **nasm** on `PATH` for the tensorstore build (`conda install -c conda-forge nasm`).
- Network access — dependencies are fetched via CMake `FetchContent`.

If you can't install system packages, get the whole toolchain from conda-forge:

```bash
conda create -n copick-cpp -c conda-forge gxx_linux-64 nasm cmake ninja
conda activate copick-cpp
```

Or `source build_env.sh` to do this in one step (creates the env once, activates it, and
exports `CC`/`CXX`).

## Build & test

```bash
# Fast: model + JSON only, no tensorstore (a few seconds).
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build
ctest --test-dir build --output-on-failure

# Full: + tensorstore (Zarr) + tinygltf (mesh). The first build compiles tensorstore.
cmake -S . -B build-full -G Ninja -DCMAKE_CXX_COMPILER=g++ \
  -DCOPICK_ENABLE_ZARR=ON -DCOPICK_ENABLE_MESH=ON
cmake --build build-full --target copick_tests
ctest --test-dir build-full --output-on-failure
```

Set `-DCMAKE_CXX_COMPILER` to your C++20 compiler if it isn't the default `g++`. Feature flags:
`-DCOPICK_ENABLE_ZARR=ON` (tensorstore), `-DCOPICK_ENABLE_MESH=ON` (tinygltf),
`-DCOPICK_BUILD_EXAMPLES=ON`.

tensorstore has no prebuilt C++ distribution, so it is compiled from source once. To avoid
recompiling it: use **ccache** (`-DCMAKE_CXX_COMPILER_LAUNCHER=ccache`) and keep `build-full/`
around. copick-cpp links only the Zarr driver + local-file kvstore, so building a specific target
does not compile tensorstore's unused drivers (gcs/grpc, aws-s3, http).

### C++11 header guard

`ctest` runs `cxx11_header_guard`, which compiles every public header under `-std=c++11` — a check
that the public API stays consumable from an old C++11 toolchain (e.g. AreTomo3's). It fails if any
C++14/17/20 construct or third-party dependency leaks into a public header. Choose the guard
compiler with `-DCOPICK_CXX11_GUARD_COMPILER=<path>` (defaults to `g++`).

## Usage

The C++ API mirrors the Python copick API (same entity hierarchy and method names):

```cpp
#include "copick/copick.h"

copick::Root root = copick::from_file("config.json");
copick::Run run = root.new_run("TS_001");                 // writes to the overlay
copick::Tomogram tomo = run.new_voxel_spacing(10.0).new_tomogram("wbp");
tomo.from_numpy(vol);                                     // OME-Zarr array + metadata
copick::Array3D slab = tomo.numpy(roi);                   // windowed read
copick::Picks picks = run.new_picks("proteasome", "0");   // point annotations (JSON)
```

Complete, runnable programs live in `examples/` (all compiled at `-std=c++11`, demonstrating C++11
consumption): `write_project` (create a project), `read_project` (read it back), and
`modify_project` (read an existing project, derive a segmentation + pick, append a run). Between
them they exercise runs, voxel spacings, tomograms, features, picks, meshes, segmentations and
objects.

Lookups (`get_run`, `get_tomogram`, `get_object`, …) return an **invalid handle** when the entity
is absent — check `handle.valid()` (or `if (handle)`). Errors throw `copick::ValidationError` /
`NotFoundError` / `PermissionError`.

### Overlay (static + overlay)

A config may point at a read-only `static_root` plus a writable `overlay_root`. copick-cpp merges
them: entities present only in the static source are read-only (writes throw `PermissionError`),
new data is written to the overlay, and the static tree is never modified.

## Cross-implementation parity

`tests/scripts/` verifies byte-compatibility with the reference Python copick (1.26.1):

```bash
# copick-cpp writes  ->  Python copick reads
./build-full/write_project /tmp/proj
python tests/scripts/verify_parity.py /tmp/proj/config.json

# Python copick writes  ->  copick-cpp reads
python tests/scripts/write_with_python.py /tmp/proj2
./build-full/read_project /tmp/proj2/config.json
```

Both directions pass, covering tomograms, segmentations, picks and meshes. Arrays are compressed
with **blosc** by default, which the numcodecs version pinned by zarr 2.x (and thus copick) can
read. `tests/scripts/fetch_sample_data.py` + `COPICK_SAMPLE_DIR` also run the `SampleData` tests
against the published sample project.

## Layout

```
include/copick/   # C++11-clean public headers (the ABI surface)
src/
  detail/         # backend impls: backend (root/run), image (vs/tomo/features), annotation, mesh
  io/             # kvstore, zarr, ome, json (dto), context — tensorstore/reflect-cpp/tinygltf here
  models/ util/   # array, validate, escape
examples/         # write_project / read_project / modify_project (C++11 consumers)
tests/
  cpp/            # GoogleTest suites
  header_guard/   # the C++11 compile guard TU
  scripts/        # Python parity + sample-data helpers
```

## Status

Implemented: full data model (Root, Run, VoxelSpacing, Tomogram, Features, Object, Picks, Mesh,
Segmentation), the filesystem backend with static/overlay merge, and Zarr/JSON/GLB I/O — all
cross-verified against Python copick. **Not yet implemented:** S3 storage (the kvstore seam is in
place), multi-level Zarr pyramids (single level is written), and the CryoET Data Portal /
mlcroissant backends. A C++20 "modern" convenience header is planned.

## License

copick-cpp is licensed under the **MIT License** — see [LICENSE](LICENSE). It matches the
upstream [copick](https://github.com/copick/copick) project.

The library builds against third-party dependencies under permissive licenses
(tensorstore — Apache-2.0; reflect-cpp, tinygltf — MIT; GoogleTest — BSD-3-Clause, tests
only). When distributing a binary that links them, include the relevant notices — see
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
