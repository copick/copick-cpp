# Consuming copick-cpp from an AreTomo-style C++11 build

A worked example of wiring copick-cpp into a tool such as **AreTomo3**, which compiles its sources at
`g++ -std=c++11` / `nvcc -std=c++11`. The goal: let AreTomo read and write copick projects natively,
without dragging copick-cpp's C++20 dependencies into AreTomo's C++11 build.

## Why it's simple: the C++11 firewall

The public headers in `include/copick/` are **strictly C++11** and include **no** third-party
headers — tensorstore, reflect-cpp and tinygltf live entirely behind a pImpl boundary inside the
compiled `libcopick`. So AreTomo's existing `-std=c++11` translation units can `#include
"copick/copick.h"` unchanged; only `libcopick` itself needs a C++20 toolchain, and it is built once,
separately.

## 1. Build & install copick-cpp (once)

Build the optimized library and install the headers + `libcopick.a` into a prefix:

```bash
cmake -S copick-cpp -B build-copick -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOPICK_ENABLE_ZARR=ON -DCOPICK_ENABLE_MESH=ON \
  -DCMAKE_INSTALL_PREFIX="$PWD/copick-prefix"
cmake --build build-copick --target copick
cmake --install build-copick
# -> copick-prefix/include/copick/*.h   and   copick-prefix/lib/libcopick.a
```

## 2. A C++11 bridge translation unit

Add one `.cpp` to AreTomo that talks to copick. It compiles with AreTomo's **existing**
`-std=c++11` — no `-std=c++20`, no dependency include paths:

```cpp
// CopickBridge.cpp — compiled at -std=c++11 alongside AreTomo's own sources.
#include <cstring>
#include "copick/copick.h"

// Write a reconstructed volume (z, y, x float32) into a copick project as an OME-Zarr tomogram.
bool copick_write_tomogram(const char* config, const char* run_name, double voxel_size,
                           const char* tomo_type, const float* vol, int nz, int ny, int nx) {
  copick::Root root = copick::from_file(config);
  copick::Run run = root.get_run(run_name);
  if (!run) run = root.new_run(run_name);                  // create if absent (writes to overlay)

  copick::Tomogram tomo = run.new_voxel_spacing(voxel_size).new_tomogram(tomo_type);

  copick::Array3D a(copick::DType::Float32, nz, ny, nx);   // (z, y, x), zero-initialized
  std::memcpy(a.data_as<float>(), vol, a.size() * sizeof(float));
  tomo.from_array(a);                                      // writes the zarr array + OME metadata
  return true;
}
```

Reading is symmetric: `copick::Array3D a = tomo.to_array();` then copy out via `a.data_as<float>()`
with `a.shape_z()/shape_y()/shape_x()`. Lookups (`get_run`, `get_tomogram`, …) return an invalid
handle when absent — test `if (!handle)`; errors throw `copick::ValidationError` / `NotFoundError` /
`PermissionError`.

Compile it exactly like the rest of AreTomo — one extra include path, nothing else:

```make
CopickBridge.o: CopickBridge.cpp
	$(GXX) -std=c++11 -I$(COPICK_PREFIX)/include -c $< -o $@
```

## 3. Linking

`libcopick.a` is a **static** library whose dependencies (tensorstore, abseil, blosc/zstd/zlib, …)
are linked **PRIVATE**, so the final executable link must also pull in that transitive closure.

**Recommended — let CMake resolve it.** If AreTomo's final link is (or can be) driven by CMake, add
copick as a subproject and the entire dependency graph is handled automatically:

```cmake
add_subdirectory(external/copick-cpp)
target_link_libraries(aretomo PRIVATE copick::copick)   # transitive deps resolved by CMake
```

**Pure Makefile.** A hand-written link line would have to enumerate copick's dependency archives,
which is brittle. The robust approach is to route just the **final link step** through the
`copick::copick` CMake target above (it emits the complete, correct link line) and hand the linked
result back to the Makefile — rather than listing tensorstore/abseil/… by hand.

## Guarantee against regressions

copick-cpp's own CI runs `cxx11_header_guard`, a CTest that compiles every public header under
`-std=c++11`. If any C++14/17/20 construct or a third-party dependency header ever leaked into the
public API, that check fails — so this exact integration path cannot silently break.
