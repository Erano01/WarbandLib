<script setup>
import Diagram from '../components/Diagram.vue'
import Callout from '../components/Callout.vue'

const anatomy = [
  {
    name: 'Engine callback scripts', status: 'api',
    note: 'the engine asks the module a question (a price, a speed, a skill modifier) or tells it that something happened (battle ended, item bought). 28 of 55 seen live. WBL_EVENT_SCRIPT_CALL.',
  },
  {
    name: 'Triggers', status: 'api', note: '34 of 43 trigger kinds seen live. WBL_EVENT_MODULE_TRIGGER for every manager dispatch.',
    children: [
      { name: 'Mission template triggers', status: 'api', note: 'hit, mount, item wielded, keys, item drop/pick-up, leave area… Agent hit also as WBL_EVENT_AGENT_HIT.' },
      { name: 'Per-trigger loops: spawn, mission start, killed, order issued, mission timers', status: 'verified', note: 'the game runs each trigger itself; needs one event per occurrence, not in the API yet' },
      { name: 'Campaign triggers and game simple triggers', status: 'api', note: 'world-map tick, simulated battles, party encounters, switch to map' },
      { name: 'Item, scene prop, presentation and map icon triggers', status: 'api', note: 'item init, presentation load/run/mouse, map icon init' },
      { name: 'Gated triggers', status: 'verified', note: 'weapon attack, missile hit, shield hit, prop hit/use: the engine dispatches only when the item or prop defines one' },
      { name: 'Siege and multiplayer triggers', status: 'mapped', note: 'scene prop animation/use/destroy, knocked down, server join/end: not exercised yet' },
    ],
  },
  {
    name: 'Directly run operation blocks', status: 'api', note: 'no trigger involved, the engine runs the block itself. WBL_EVENT_OPERATION_BLOCK.',
    children: [
      { name: 'Dialog lines: condition and consequence', status: 'api' },
      { name: 'Game menus: on-load, option conditions, option consequence', status: 'api' },
      { name: 'Tableau materials (banners, shields, portraits)', status: 'verified' },
    ],
  },
  { name: 'Trigger parameters and result', status: 'api', note: 'shared globals: copied into each payload at entry, written back explicitly' },
  { name: 'Threads', status: 'api', note: 'all of the above on the game thread; run_on_game_thread for everything else' },
]

const statusLabel = { mapped: 'mapped (static)', verified: 'verified live', api: 'in the API' }

const families = `
flowchart LR
  subgraph Engine
    Q["engine question / notification"]
    T["engine event point"]
    B["menu · dialog · tableau"]
  end
  Q --> CS["callback script"]
  T --> G{"gated?"}
  G -- "defined" --> TR["trigger dispatch"]
  G -- "not defined" --> X["nothing runs"]
  B --> OB["operation block"]
  CS --> I["module interpreter"]
  TR --> I
  OB --> I
  I --> R["result read back by the engine"]
`

const threads = `
sequenceDiagram
  participant GT as Game thread
  participant RT as Render thread
  participant P as Plugin
  GT->>GT: simulation, triggers, menus, dialogs, module code
  GT->>P: game events: synchronous, inside the dispatch
  RT->>P: FRAME / DRAW / UI
  Note over GT,RT: no synchronisation between the two
  P-->>GT: run_on_game_thread: applied between frames
`
</script>

<template>
  <h1>Game events</h1>
  <p class="lead">How the game reacts to its own events, what WarbandLib has verified about it, and the event API built on top. This page describes behaviour and design only; the per-build data behind it lives in the private catalogue (<RouterLink to="/game-data">game data model</RouterLink>).</p>

  <h2>The WarbandLib Event Bus</h2>
  <p>One central, priority-ordered bus, closest to Forge's event bus: every event has an id, anyone can subscribe, PRE can cancel, plugins register their own named events. It has two layers. The <strong>bus</strong> itself (subscription, priorities, cancellation, custom events) has existed since ABI 1.0. <strong>Game events</strong> (ABI 1.1) are produced from the game's own dispatch, the way CraftBukkit raises Bukkit events from inside the server. A per-event callback object, as in Fabric, would need a function table per event; a single <code>subscribe</code> with numeric ids and <code>struct_size</code>-prefixed payloads keeps the C ABI small and stable.</p>

  <h2>Decision</h2>
  <p>WarbandLib does not invent its own event points. It listens to the dispatch the game already performs, so an event fires exactly when the game itself (and any module code) reacts, with the game's own data. What plugins see is a cleaner layer on top: typed payloads, one event per real occurrence, explicit phases and a fixed thread contract.</p>

  <h2>Anatomy</h2>
  <ul class="atlas">
    <li v-for="o in anatomy" :key="o.name">
      <span class="badge" :class="o.status">{{ statusLabel[o.status] }}</span> <strong>{{ o.name }}</strong> <span class="note">— {{ o.note }}</span>
      <ul v-if="o.children">
        <li v-for="t in o.children" :key="t.name">
          <span class="badge" :class="t.status">{{ statusLabel[t.status] }}</span> {{ t.name }} <span v-if="t.note" class="note">— {{ t.note }}</span>
        </li>
      </ul>
    </li>
  </ul>

  <Diagram :code="families" caption="The three routes from the engine into module code." />

  <h2>How it was verified</h2>
  <p>Static analysis mapped every dispatch route; a probe plugin then counted each route live, per thread, with nesting depth and whether parameters changed underneath. Sessions covered a quick battle, the world map with its menus, trade and party screens, and town, village, castle and training-field scenes.</p>

  <h2>Findings that shape the API</h2>
  <ul>
    <li><strong>One thread.</strong> All dispatch and all module code run on the game thread. Nothing in the game calls module code from elsewhere.</li>
    <li><strong>Render has no fixed thread.</strong> Present runs mostly on a separate render thread, but also on the game thread during screen transitions. Render-side code must not assume either.</li>
    <li><strong>Parameters are shared globals and unprotected.</strong> An engine callback made inside a trigger overwrites them, and timed passes see whatever the previous event left behind.</li>
    <li><strong>Some loops fire once per trigger.</strong> For spawn, mission start, killed or order issued, the game runs each trigger in turn, so one occurrence shows up many times at the lowest level.</li>
    <li><strong>Gated events are invisible to Native.</strong> Weapon attack, missile hit and shield hit happen all the time, but the game only dispatches them when an item defines a trigger, which Native never does.</li>
    <li><strong>Menus and dialogs are not triggers.</strong> They are blocks the game runs itself, so they need their own event points.</li>
  </ul>

  <h2>Event API (ABI 1.1)</h2>
  <table>
    <thead><tr><th>Event</th><th>From</th><th>Can change</th></tr></thead>
    <tbody>
      <tr><td><code>WBL_EVENT_MODULE_TRIGGER</code></td><td>Every trigger manager dispatch: mission, campaign, game simple, object triggers. Timed passes arrive with context 0 and no params.</td><td>Params (PRE), result (PRE, POST); cancel skips module triggers.</td></tr>
      <tr><td><code>WBL_EVENT_SCRIPT_CALL</code></td><td>The engine calling a <code>game_*</code> script, defined by the module or not.</td><td>Arguments (PRE), result (PRE, POST); cancel behaves like an undefined script.</td></tr>
      <tr><td><code>WBL_EVENT_OPERATION_BLOCK</code></td><td>Dialog line condition/consequence, menu opened, menu option condition/consequence.</td><td>The outcome (POST); cancel skips the block and returns the outcome chosen in PRE.</td></tr>
      <tr><td><code>WBL_EVENT_AGENT_HIT</code></td><td>The agent-hit dispatch of a mission, typed.</td><td>Damage (PRE, POST); cancel skips module triggers.</td></tr>
    </tbody>
  </table>
  <ul>
    <li><strong>Phases.</strong> Subscribe to <code>WBL_PRE(id)</code> or <code>WBL_POST(id)</code>. PRE runs before module code, POST after it; a PRE cancel skips both module code and POST.</li>
    <li><strong>Snapshot at entry.</strong> The payload is a copy taken when the event starts; changes are written back explicitly, so nested dispatch can't corrupt a handler's data.</li>
    <li><strong>No stale data.</strong> Timed passes carry no params, because the globals hold the previous event's leftovers.</li>
    <li><strong>No addresses in plugins.</strong> The runtime finds the dispatch points in the game data for the running exe; plugins only see ids and payloads. See <RouterLink to="/reference">the reference</RouterLink> for every field.</li>
  </ul>

  <h3>Not in the API yet</h3>
  <ul>
    <li>Typed events for the per-trigger loops (spawn, mission start/end, killed, order issued), raised once per occurrence.</li>
    <li>Gated events raised at the gate (weapon attack, missile hit, shield hit, scene prop hit/use), so they fire whatever the module defines.</li>
    <li>Ids in block payloads: which menu, which option, which dialog line.</li>
    <li>Tableau material events, and typed campaign events (encounter, simulated battle, map tick).</li>
  </ul>

  <h2>Thread contract</h2>
  <Diagram :code="threads" caption="Game events stay on the game thread; render-side writes are marshalled to it." />
  <p>Game events run on the game thread while the game waits. FRAME, DRAW and UI run wherever the game presents, concurrently with the simulation, so they treat game state as read-only and hand changes to <code>run_on_game_thread</code>. Queued tasks run once, in order, on the game thread between frames (in its window message loop). <code>example.hp_editor</code> works this way since runtime 0.2.</p>

  <h2>Still open</h2>
  <ul>
    <li>Live verification of the ABI 1.1 events in battle, on the map and in dialogs.</li>
    <li>Siege-only and multiplayer events: not exercised yet.</li>
    <li>Whether render and game threads share any synchronisation point. This decides what render-side reads can be declared safe.</li>
  </ul>
</template>

<style scoped>
.atlas, .atlas ul { list-style: none; padding-left: 18px; }
.atlas { padding-left: 0; }
.atlas li { margin: 6px 0; }
.note { color: var(--muted); font-size: 14px; }
.badge { display: inline-block; font: 11px var(--mono); padding: 1px 7px; border-radius: 10px; border: 1px solid var(--border); margin-right: 4px; }
.badge.verified { background: color-mix(in srgb, var(--ok) 18%, transparent); border-color: var(--ok); }
.badge.mapped { background: color-mix(in srgb, var(--warn) 15%, transparent); border-color: var(--warn); }
.badge.api { background: var(--accent-soft); border-color: var(--accent); }
</style>
