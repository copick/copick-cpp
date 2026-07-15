# Third-party licenses

copick-cpp itself is licensed under the [MIT License](LICENSE). It builds against the
libraries below, which are fetched at configure time and (except googletest) linked into
any binary you build. Their licenses are permissive and compatible with MIT; when you
**distribute a binary** that links these, include the relevant notices below.

| Dependency | Version | License | Copyright | Linked into `libcopick` |
|---|---|---|---|---|
| [tensorstore](https://github.com/google/tensorstore) | v0.1.84 | Apache-2.0 | © The TensorStore Authors | yes (Zarr I/O) |
| [reflect-cpp](https://github.com/getml/reflect-cpp) | v0.25.0 | MIT | © 2023–2025 Code17 GmbH | yes (JSON) |
| [tinygltf](https://github.com/syoyo/tinygltf) | v2.9.7 | MIT | © 2017 Syoyo Fujita, Aurélien Chatelain and contributors | yes, opt-in (GLB mesh) |
| [GoogleTest](https://github.com/google/googletest) | v1.17.0 | BSD-3-Clause | © Google Inc. | no (tests only) |

Notes:

- **tensorstore is Apache-2.0.** When redistributing a binary that links it, include the
  [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0) and reproduce
  tensorstore's `NOTICE`/`LICENSE`. tensorstore in turn vendors its own dependencies
  (abseil, riegeli, blosc, zstd, zlib, nlohmann/json, …) under their respective permissive
  licenses; see tensorstore's `LICENSE` and `third_party/` for the complete list.
- **tinygltf** bundles `nlohmann/json` (MIT) and `stb_image`/`stb_image_write`
  (public domain / MIT). We build it with the STB image features disabled.
- **GoogleTest** is used only to build and run the test suite; it is not part of the
  distributed library.
