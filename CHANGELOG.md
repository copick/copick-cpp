# Changelog

## [0.3.0](https://github.com/copick/copick-cpp/compare/v0.2.0...v0.3.0) (2026-10-08)


### ⚠ BREAKING CHANGES

* Point::instance_id changes from int to std::int64_t, which changes the layout of Point and the ABI. Consumers (AreTomo3) rebuild; code that assigns an int keeps compiling.

### Features

* 64-bit instance ids and filament spec accessor ([#2](https://github.com/copick/copick-cpp/issues/2)) ([98a7cd7](https://github.com/copick/copick-cpp/commit/98a7cd7c9b00919e50fe54438c00c6b44c6b4def))

## [0.2.0](https://github.com/copick/copick-cpp/compare/v0.1.0...v0.2.0) (2026-07-15)


### Features

* core copick C++ API with local filesystem backend ([25f238e](https://github.com/copick/copick-cpp/commit/25f238e5d76b88a85a817bdd6ff366a81fd03f50))
* cut first tagged release (0.2.0) ([49d661b](https://github.com/copick/copick-cpp/commit/49d661bf1e65eed20bcaeea8dd6b7bb19f4bf813))
* Release build config + AreTomo integration guide ([9658a23](https://github.com/copick/copick-cpp/commit/9658a23fce116d867c4529f0e4019c9bf2e9c6c5))
* rename array accessors numpy/from_numpy to to_array/from_array ([de484b9](https://github.com/copick/copick-cpp/commit/de484b9a2ee098cb249456b79a35e393d9d72f60))


### Bug Fixes

* make version smoke test resilient to release-please bumps ([7902f7c](https://github.com/copick/copick-cpp/commit/7902f7c1ab28387ba66085506d6072786e4d37b3))
