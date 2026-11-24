# WarbandLib

An open-source plugin runtime and stable C ABI for Mount & Blade: Warband. The goal is a modding ecosystem in the spirit of Minecraft/Forge, where mods written by different people with different compilers run side by side in the same game. The first large target built on top of it is a campaign co-op mod.

- A single C header (`sdk/include/warbandlib/warbandlib.h`), versioned and backward compatible
- Plugins loaded in dependency order, prioritized events and shared hook chains
- Services between plugins (interfaces) and an in-game menu/drawing API
- Builds the same way from Linux and Windows

## Setup

Requires CMake ≥ 3.21. On Linux also clang-cl, lld, Ninja and [xwin](https://github.com/Jake-Shadle/xwin) (`~/toolchains/xwin-x86`); on Windows, Visual Studio 2022.

```bash
# Linux
cmake --preset build-release-clangcl-x86
cmake --build --preset build-release-clangcl-x86

# Windows
cmake --preset windows-msvc-x86
cmake --build --preset windows-msvc-x86-release
```

Output goes to `build/<preset>/bin/`: `warbandlib_runtime.dll`, `injector.exe` and `plugins/`.

## Documentation

Setup details, architecture, writing plugins and the API reference are on the documentation site:

```bash
cd docs
npm install
npm run dev
```

## License

[LICENSE](LICENSE)
