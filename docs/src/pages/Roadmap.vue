<template>
  <h1>Roadmap</h1>
  <p class="lead">Where WarbandLib is going, and what the first large plugin on top of it, campaign co-op, needs from it.</p>

  <h2>Done (runtime 0.1, ABI 1.0)</h2>
  <ul>
    <li>Stable C ABI with compile-time layout lock</li>
    <li>Plugin loader: ids, versions, required and optional dependencies, load order, crash-guarded calls</li>
    <li>Event bus with priorities and custom events, versioned interfaces between plugins</li>
    <li>Shared, priority-ordered inline hook chains</li>
    <li>Fault-safe memory access and pattern search</li>
    <li>Overlay with C menu and draw api; raw D3D9 frame event</li>
    <li>Same build from Linux (clang-cl + xwin) and Windows (Visual Studio)</li>
  </ul>

  <h2>Done (runtime 0.2, ABI 1.1)</h2>
  <ul>
    <li>Game events from the game's own dispatch with PRE/POST phases: module triggers, engine script calls, dialog and menu blocks, agent hit (<RouterLink to="/game-events">details</RouterLink>)</li>
    <li>Game-thread task queue for changing game state from menus and other threads</li>
    <li>Game data (symbols) read per exe at runtime, never compiled into WarbandLib</li>
  </ul>

  <h2>Next</h2>
  <ul>
    <li>Symbol catalogue and local generation, replacing raw addresses (<RouterLink to="/game-data">design</RouterLink>)</li>
    <li>Loader without an injector, starting with the game</li>
    <li>Typed game objects: agents, missions, parties, items, exposed as stable handles</li>
    <li>More typed game events: mission start/end, spawn, killed, party encounter, gated combat events, menu and dialog ids (<RouterLink to="/game-events">status</RouterLink>)</li>
    <li>Content registry: namespaced item, troop and party-template ids merged at load time, so content from several plugins can live in one game</li>
    <li>Module-system bridge: new script operations registered by plugins</li>
  </ul>

  <h2>Campaign co-op on top</h2>
  <p>Warband's battles have native networking, its campaign has none. A co-op plugin set would be split along WarbandLib interfaces: a transport plugin, a host-authoritative world-state plugin replicating time, parties and the economy, an interaction plugin locking dialogs and trades per player, and a battle plugin reusing or replicating mission state. Each needs the typed objects and gameplay events above, which is why they come first.</p>
</template>
