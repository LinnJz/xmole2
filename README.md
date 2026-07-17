# xmole2: Office Document Processing in Modern C++

[English](README.md) | [简体中文](README.zh-CN.md)

[![Language](https://img.shields.io/badge/language-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Build System](https://img.shields.io/badge/build-CMake-064F8C.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

xmole2 is an open-source C++23 library for reading, editing, writing, and converting Microsoft Office documents. It is designed as a family of independent Words, Cells, and Slides components built on shared packaging, XML, security, diagnostics, and I/O infrastructure.

> [!IMPORTANT]
> xmole2 is in early development. The architecture and foundational APIs are being established; Office document codecs and the public document object models are not ready for production use.

## Design goals

- Support modern OOXML containers and legacy compound-file formats without conflating OPC and CFB/OLE2 semantics.
- Provide separate `WordDocument`, `Workbook`, and `Presentation` domain models with a consistent document lifecycle.
- Preserve unsupported and vendor-specific content during same-format round trips whenever it is safe to do so.
- Scale to large workbooks through indexed package access, lazy part loading, and streaming worksheet readers.
- Treat Office files as untrusted input and apply shared resource limits throughout parsing, editing, calculation, rendering, and saving.
- Keep public APIs independent of third-party implementation types.
- Remain portable across Windows, Linux, and macOS.

## Project status

The repository currently provides:

- the accepted architecture and engineering specifications;
- independent CMake targets for the planned module boundaries;
- the initial `xmole2::base` error, result, resource-budget, cancellation, and operation-context facilities;
- foundational contract tests and CMake package exports.

Document loading, document models, format conversion, calculation, and rendering are still under development. Usage examples and a detailed feature matrix will be added when those APIs are implemented and stable enough to document accurately.

## Requirements

- A C++23 compiler and standard library with `std::expected` support
- CMake 3.28 or later
- Ninja or another CMake-supported generator

Development is currently validated with MSVC 19.44 on Windows x64. Linux x64 and macOS support are architectural requirements and will be enabled as the corresponding platform contracts are implemented. Older toolchains that accept a C++23 language mode but do not provide the required C++23 standard-library facilities are not supported.

## Dependencies

Dependency metadata, pinned versions, upstream repositories, and license identifiers are maintained in [`deps.json`](deps.json). The entries describe dependencies available to xmole2 modules; the early foundational targets do not necessarily link every library yet. All third-party dependencies are private implementation details and must not appear in xmole2 public APIs.

[`vcpkg.json`](vcpkg.json) pins the registry baseline used to reproduce these versions. BqLog remains an optional local integration because it is not currently available from the selected builtin registry baseline.

| Library | Version | Purpose | License |
|---|---:|---|---|
| [Abseil](https://abseil.io/) | 20250814.1 | Optimized containers and internal utilities | Apache-2.0 |
| [Boost.UUID](https://www.boost.org/doc/libs/1_90_0/libs/uuid/) | 1.90.0#1 | UUID generation and representation | BSL-1.0 |
| [BqLog](https://github.com/Tencent/BqLog) | 2.3.1 | Logging backend | Apache-2.0 |
| [fmt](https://fmt.dev/) | 12.1.0 | Internal string formatting and diagnostics | MIT |
| [frozen](https://github.com/serge-sans-paille/frozen) | 1.2.0 | Compile-time immutable lookup tables | Apache-2.0 |
| [GoogleTest](https://github.com/google/googletest) | 1.17.0#2 | Test framework | BSD-3-Clause |
| [magic_enum](https://github.com/Neargye/magic_enum) | 0.9.7#1 | Compile-time enum utilities | MIT |
| [minizip-ng](https://github.com/zlib-ng/minizip-ng) | 4.0.10#1 | ZIP container backend | BSD-3-Clause |
| [OpenSSL](https://www.openssl.org/) | 3.6.0#3 | Cryptographic primitives | Apache-2.0 |
| [pugixml](https://pugixml.org/) | 1.15#1 | XML parsing backend | MIT |
| [RE2](https://github.com/google/re2) | 2025-11-05 | Regular-expression processing | BSD-3-Clause |
| [simdutf](https://simdutf.github.io/simdutf/) | 8.0.0 | SIMD Unicode validation and transcoding for XML input | Apache-2.0 |

The `#N` suffix used by some versions is the vcpkg port-version suffix. When adding, removing, or upgrading a dependency, update `deps.json` first and review the upstream license before changing build integration.

## Build

Copy `CMakeUserPresets.json.example` to the ignored `CMakeUserPresets.json`, edit it with your local paths, then list and use the available presets:

```console
cmake --list-presets all -S .
```

Pick any listed configure preset and run:

```console
cmake --preset <preset_name>
cmake --build --preset build-<preset_name>
ctest --test-dir "out/build/<preset_name>" --output-on-failure
```

> **Note:** CMakeUserPresets.json may contain `//` or `/* */` comments (JSON5) for guidance. If your toolchain or editor rejects them, remove all comments before use.

For manual configuration, make sure CMake resolves a compiler and standard library that provide `std::expected`:

```console
cmake -S . -B out/build/default -G Ninja -DCMAKE_BUILD_TYPE=Debug -DXMOLE2_BUILD_TESTS=ON
cmake --build out/build/default
ctest --test-dir out/build/default --output-on-failure
cmake --install out/build/default --prefix out/install/default
```

## Documentation

- [Specification index](docs/spec/README.md)
- [Architecture](docs/spec/architecture.md)
- [Public API and lifetime model](docs/spec/public-api.md)
- [Fidelity and conversion](docs/spec/fidelity.md)
- [Security and resource limits](docs/spec/security.md)
- [Dependencies and source-inspection guide](docs/spec/dependencies.md)
- [C++ code style](docs/spec/code-style.md)
- [Testing and quality requirements](docs/spec/testing.md)
- [Local fixture catalog](docs/fixtures/catalog.md)
- [Local reference snapshots](docs/reference-snapshots.md)
- [Architecture decision records](docs/adr/)

The complete architecture discussion is preserved in [the design-session record](docs/meetings/2026-07-15-office-architecture-all-sessions.md). Changes to architectural boundaries or public contracts require an ADR and corresponding specification and test updates.

## Contributing

Contributions are welcome while the project is taking shape. See [CONTRIBUTING.md](CONTRIBUTING.md). Before changing module boundaries or public contracts, read the specifications above and record the decision in an ADR. C++ changes must follow the repository style guide and be formatted with the checked-in `.clang-format` configuration.

## License

xmole2 is licensed under the [Apache License 2.0](LICENSE).
