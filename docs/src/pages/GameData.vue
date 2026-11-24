<script setup>
import CodeBlock from '../components/CodeBlock.vue'
import Callout from '../components/Callout.vue'
const flow = `
# on the developer's (or player's) machine, once per game build
wbl-buildtools --game "C:/.../MountBlade Warband/mb_warband.exe"
  1. hash mb_warband.exe, look up the build in the catalogue
  2. resolve every catalogue symbol in *this* exe
  3. write  warband_symbols.<sha256>.bin  next to the runtime
  4. refuse (and say which symbols failed) instead of guessing`
const plugin = `
uintptr_t set_hp;
if (host->symbols->resolve("agent.set_hit_points", &set_hp) == WBL_OK) {
    host->hooks->create(self, (void*)set_hp, ...);
}`
</script>

<template>
  <h1>Game data without shipping internals</h1>
  <p class="lead">Design for the next ABI minor. Not implemented yet: today examples still use <code>rebase()</code> with addresses written in their source.</p>

  <h2>The problem</h2>
  <p>WarbandLib is open source, but it must not become a catalogue of the game's addresses, and it must not turn into a reverse-engineering tool. At the same time plugins need to reach game functions and structures, and Warband exists in several builds (Steam, GOG, different patch levels) whose addresses differ.</p>

  <h2>The model: build-time generation on the user's machine</h2>
  <p>Like Spigot's BuildTools, which builds the server from the user's own copy of the game instead of redistributing it, the per-build address table is never published. It is generated locally from the exe the user already owns:</p>
  <CodeBlock :code="flow" lang="bash" />
  <p>Plugins never see raw addresses in their source. They ask for stable symbol names, and the same plugin works on every build the catalogue covers:</p>
  <CodeBlock :code="plugin" lang="c" />

  <h2>What lives where</h2>
  <table>
    <thead><tr><th></th><th>Published</th><th>Generated locally</th></tr></thead>
    <tbody>
      <tr><td>Runtime, SDK, plugin API</td><td>yes</td><td></td></tr>
      <tr><td>Symbol catalogue (stable names + how to locate each in a build)</td><td>separate data package</td><td></td></tr>
      <tr><td>Addresses for a specific exe</td><td>never</td><td><code>warband_symbols.&lt;sha256&gt;.bin</code></td></tr>
      <tr><td>Game binary, decompiled code</td><td>never</td><td>never</td></tr>
      <tr><td>How symbols were found</td><td>never</td><td>never</td></tr>
    </tbody>
  </table>

  <h2>Why it is not a reversing tool</h2>
  <p>The generator only matches entries already in the catalogue against one exe and verifies them. It has no disassembler, decompiler, scanner UI or exploration mode, and refuses unknown builds. Finding new symbols is maintainers' work done elsewhere; WarbandLib only consumes the result.</p>

  <Callout kind="warn" title="What this does not do">
    <p>It does not make the information secret. The catalogue has to describe each symbol somehow, and anyone can run the generator. The model removes addresses and game code from the repository, keeps plugins independent of the build, and keeps the methodology out of the project, which is what an open-source framework can realistically guarantee.</p>
  </Callout>
</template>
