# --- C++11 public-header guard -------------------------------------------------
# Compile a TU that includes every public header under -std=c++11 with the guard compiler
# (COPICK_CXX11_GUARD_COMPILER). Enforces the AreTomo3 constraint: the headers must be
# consumable from a plain C++11 translation unit, with none of the C++17/20 deps
# leaking through. This is a compile-only (-fsyntax-only) check.
add_test(
  NAME cxx11_header_guard
  COMMAND ${COPICK_CXX11_GUARD_COMPILER}
          -std=c++11 -fsyntax-only -Wall -Wextra
          -I "${CMAKE_SOURCE_DIR}/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/header_guard/cxx11_include_all.cpp"
)

# --- GoogleTest suite ----------------------------------------------------------
include(GoogleTest)

add_executable(copick_tests
  cpp/smoke_test.cpp
  cpp/escape_test.cpp
  cpp/object_models_test.cpp
  cpp/json_test.cpp
  cpp/array_test.cpp
)
target_link_libraries(copick_tests PRIVATE copick GTest::gtest_main)
target_compile_features(copick_tests PRIVATE cxx_std_20)
set_target_properties(copick_tests PROPERTIES CXX_EXTENSIONS OFF)

# Zarr-backed tests exercise the internal io/ layer directly, so they need src/ on the
# include path and are only built when the tensorstore backend is enabled.
if(COPICK_ENABLE_ZARR)
  target_sources(copick_tests PRIVATE
    cpp/zarr_io_test.cpp
    cpp/backend_test.cpp
    cpp/image_test.cpp
    cpp/annotation_test.cpp
    cpp/mesh_test.cpp
    cpp/overlay_test.cpp
    cpp/sample_data_test.cpp
  )
  target_include_directories(copick_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
  if(COPICK_ENABLE_MESH)
    target_compile_definitions(copick_tests PRIVATE COPICK_ENABLE_MESH)
  endif()
endif()

gtest_discover_tests(copick_tests)
