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

## Run

Start the game first, then inject the runtime. It loads every DLL in `plugins/` on its own. Injecting at the main menu or on the campaign map are both fine.

**Linux (Steam + Proton).** One-time: install `protonhax` (AUR) and set Warband's Steam launch options to `protonhax init %COMMAND%`. Then, with the game running:

```bash
tools/hot-reload.sh     # unload any live copy, inject a fresh one
tools/unload.sh         # unload cleanly
```

`hot-reload.sh` copies the runtime and plugins to `build/<preset>/bin/live/<HHMMSS>/` and injects that copy, so rebuilding and re-running it never fights a DLL the game still holds. The log of each copy is `live/<HHMMSS>/warbandlib_runtime.dll.log`. The script prints its path.

**Windows.**

```bat
injector.exe mb_warband.exe C:\path\to\bin\warbandlib_runtime.dll
injector.exe --eject mb_warband.exe warbandlib_runtime.dll
```

The log is `warbandlib_runtime.dll.log` next to the DLL.

In game, **INSERT** opens the WarbandLib menu with every plugin, its version and the reason it failed if it did. If nothing opens, check the log. When it has no `ready:` line, the injection did not happen. Always unload with `unload.sh` or `--eject`. Both ask the runtime to shut down by itself, which is the only point where plugins can be unloaded safely.

## Documentation

Setup details, architecture, writing plugins and the API reference are on the documentation site:

```bash
cd docs
npm install
npm run dev
```

## License

[LICENSE](LICENSE)
