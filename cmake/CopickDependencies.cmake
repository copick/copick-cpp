# Third-party dependencies, fetched and pinned via FetchContent.
#
# Heavy deps are gated behind feature options so early phases (model + JSON only)
# configure/build fast without compiling tensorstore.
#
#   reflect-cpp  — JSON (de)serialization + validation (C++20, header-only)   [always]
#   tinygltf     — GLB mesh I/O (C++11, header-only)          [COPICK_ENABLE_MESH]
#   tensorstore  — Zarr v2/v3 + file/s3 kvstore (C++17)       [COPICK_ENABLE_ZARR]
#   googletest   — test framework                             [COPICK_BUILD_TESTS]

include(FetchContent)

# ---------------------------------------------------------------------------
# reflect-cpp (JSON + validation). Header-only; used only inside src/ (C++20).
# ---------------------------------------------------------------------------
FetchContent_Declare(
  reflectcpp
  GIT_REPOSITORY https://github.com/getml/reflect-cpp.git
  GIT_TAG        v0.25.0
  GIT_SHALLOW    TRUE
)
# reflect-cpp bundles its own JSON backend (yyjson) and builds a small static lib.
set(REFLECTCPP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(reflectcpp)

# ---------------------------------------------------------------------------
# tinygltf (GLB mesh I/O). Header-only C++11; consumed only inside src/.
# ---------------------------------------------------------------------------
if(COPICK_ENABLE_MESH)
  FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.9.7
    GIT_SHALLOW    TRUE
  )
  # We only need the header; skip its loader/examples/tests.
  set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
  set(TINYGLTF_INSTALL OFF CACHE BOOL "" FORCE)
  set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(tinygltf)
endif()

# ---------------------------------------------------------------------------
# tensorstore (Zarr I/O). Built as its own C++17 target; ~long first build.
# ---------------------------------------------------------------------------
if(COPICK_ENABLE_ZARR)
  FetchContent_Declare(
    tensorstore
    GIT_REPOSITORY https://github.com/google/tensorstore.git
    GIT_TAG        v0.1.84
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(tensorstore)
endif()

# ---------------------------------------------------------------------------
# googletest (tests only).
# ---------------------------------------------------------------------------
if(COPICK_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
    GIT_SHALLOW    TRUE
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()
