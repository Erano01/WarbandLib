<script setup>
import CodeBlock from '../components/CodeBlock.vue'
import Callout from '../components/Callout.vue'
const linuxSetup = `
# Arch Linux; package names differ elsewhere
sudo pacman -S cmake ninja clang lld llvm
cargo install xwin
xwin --accept-license --arch x86 splat --output ~/toolchains/xwin-x86`
const linuxBuild = `
cmake --preset build-release-clangcl-x86
cmake --build --preset build-release-clangcl-x86
# unit tests run natively
cmake --preset linux-host-tests && cmake --build --preset linux-host-tests
ctest --preset linux-host-tests`
const winBuild = `
cmake --preset windows-msvc-x86
cmake --build --preset windows-msvc-x86-release
# or the ClangCL toolset
cmake --preset windows-clangcl-x86
cmake --build --preset windows-clangcl-x86-release`
const layout = `
bin/
  warbandlib_runtime.dll     the runtime, injected into mb_warband.exe
  warbandlib_runtime.dll.log its log
  mb_warband.ini             known game builds
  injector.exe
  plugins/
    example_hello_c.dll
    example_hp_editor.dll
    example_overlay_quad.dll
    example.hello_c/         per-plugin data directory`
const inject = `
# Linux, game started from Steam with: protonhax init %COMMAND%
tools/hot-reload.sh            # unload any live copy, inject a fresh one
tools/unload.sh                # unload cleanly

# Windows
injector.exe mb_warband.exe C:\\path\\to\\bin\\warbandlib_runtime.dll
injector.exe --eject mb_warband.exe warbandlib_runtime.dll`
</script>

<template>
  <h1>Getting started</h1>
  <p class="lead">The game is a 32-bit Windows program, so everything is built for x86 Windows, from either operating system.</p>

  <h2>Requirements</h2>
  <table>
    <thead><tr><th></th><th>Linux</th><th>Windows</th></tr></thead>
    <tbody>
      <tr><td>Compiler</td><td>clang-cl + lld-link (LLVM), targeting i686-pc-windows-msvc</td><td>Visual Studio 2022 (MSVC or the ClangCL toolset)</td></tr>
      <tr><td>Windows headers and CRT</td><td>xwin, splatted to <code>~/toolchains/xwin-x86</code></td><td>Windows SDK from Visual Studio</td></tr>
      <tr><td>Build</td><td>CMake ≥ 3.21, Ninja</td><td>CMake ≥ 3.21</td></tr>
      <tr><td>Running the game</td><td>Steam with Proton, <code>protonhax</code> for injecting</td><td>Steam</td></tr>
    </tbody>
  </table>
  <p>ImGui 1.60, MinHook 1.3.4 and, if not installed, GoogleTest are fetched by CMake on first configure.</p>

  <h2>Build on Linux</h2>
  <CodeBlock :code="linuxSetup" lang="bash" title="one-time setup" />
  <CodeBlock :code="linuxBuild" lang="bash" />
  <Callout kind="note" title="Different xwin location">
    <p>Pass <code>-DXWIN_DIR=/your/path</code> on the first configure.</p>
  </Callout>

  <h2>Build on Windows</h2>
  <CodeBlock :code="winBuild" lang="bash" title="Developer PowerShell for VS 2022" />

  <h2>What you get</h2>
  <CodeBlock :code="layout" lang="bash" title="build/<preset>/bin" />

  <h2>Run it</h2>
  <CodeBlock :code="inject" lang="bash" />
  <p>Press <strong>INSERT</strong> in game for the WarbandLib menu: it lists every plugin, its version and why it failed if it did. Everything the runtime does is in <code>warbandlib_runtime.dll.log</code>.</p>
  <Callout kind="warn" title="Unloading">
    <p>Always unload through <code>tools/unload.sh</code> or <code>injector --eject</code>. Both ask the runtime to shut down by itself (the <code>.unload</code> flag file), which is the only point where plugins can be unloaded safely.</p>
  </Callout>
</template>
