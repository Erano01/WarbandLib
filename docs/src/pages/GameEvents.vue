<script setup>
import Diagram from '../components/Diagram.vue'
import Callout from '../components/Callout.vue'

const anatomy = [
  {
    name: 'Engine callback scripts', status: 'verified',
    note: 'the engine asks the module a question (a price, a speed, a skill modifier) or tells it that something happened (battle ended, item bought). 28 of 55 seen live.',
  },
  {
    name: 'Triggers', status: 'verified', note: '34 of 43 trigger kinds seen live',
    children: [
      { name: 'Mission template triggers', status: 'verified', note: 'agent spawn, hit, killed, mount, item wielded, keys, order issued, leave area…' },
      { name: 'Campaign triggers and game simple triggers', status: 'verified', note: 'world-map tick, simulated battles, party encounters, switch to map' },
      { name: 'Item, scene prop, presentation and map icon triggers', status: 'verified', note: 'item init, presentation load/run/mouse, map icon init' },
      { name: 'Gated triggers', status: 'verified', note: 'weapon attack, missile hit, shield hit, prop hit/use: the engine dispatches only when the item or prop defines one' },
      { name: 'Siege and multiplayer triggers', status: 'mapped', note: 'scene prop animation/use/destroy, knocked down, server join/end: not exercised yet' },
    ],
  },
  {
    name: 'Directly run operation blocks', status: 'verified', note: 'no trigger involved, the engine runs the block itself',
    children: [
      { name: 'Dialog lines: condition and consequence', status: 'verified' },
      { name: 'Game menus: on-load, option conditions, option consequence', status: 'verified' },
      { name: 'Tableau materials (banners, shields, portraits)', status: 'verified' },
    ],
  },
  { name: 'Trigger parameters and result', status: 'verified', note: 'shared globals: they carry the payload in, the module\'s answer out' },
  { name: 'Threads', status: 'verified', note: 'all of the above on the game thread' },
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
  GT->>P: game events (planned): synchronous, inside the dispatch
  RT->>P: FRAME / DRAW / UI (today)
  Note over GT,RT: no synchronisation between the two
  P-->>GT: writes queued, applied on the game thread (planned)
`
</script>

<template>
  <h1>Game events</h1>
  <p class="lead">How the game reacts to its own events, what WarbandLib has verified about it, and the event API being built on top. This page describes behaviour and design only; the per-build data behind it lives in the private catalogue (<RouterLink to="/game-data">game data model</RouterLink>).</p>

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

  <h2>Event API design (planned)</h2>
  <ul>
    <li><strong>Typed events.</strong> Each event has its own payload struct (agent hit: victim, attacker, damage, item, position…), versioned with <code>struct_size</code> like the rest of the ABI.</li>
    <li><strong>Pre and post phases.</strong> PRE runs before module code and may change the input or cancel; POST runs after and sees the result the game will use.</li>
    <li><strong>Snapshot at entry.</strong> The payload is copied when the event starts and results are written back explicitly, so nested dispatch can't corrupt a handler's data.</li>
    <li><strong>One event per occurrence.</strong> Per-trigger loops are collapsed. Timed passes become tick events with no payload.</li>
    <li><strong>Gated events raised at the gate</strong>, from the engine's own data, so they work whatever the module defines.</li>
    <li><strong>Menus, dialogs and callback queries as first-class events.</strong> Examples: menu opened, option chosen, dialog answered, price asked.</li>
  </ul>

  <h2>Thread contract</h2>
  <Diagram :code="threads" caption="Game events stay on the game thread; render-side writes are marshalled to it." />
  <Callout kind="warn" title="Known issue">
    <p>Today the FRAME/DRAW/UI events run on the render thread, concurrently with the simulation. The <code>example.hp_editor</code> plugin changes agent state from its menu, which is a data race. It will move to the queued-write API once that exists.</p>
  </Callout>

  <h2>Still open</h2>
  <ul>
    <li>Siege-only and multiplayer events: not exercised yet.</li>
    <li>Whether render and game threads share any synchronisation point. This decides what render-side reads can be declared safe.</li>
    <li>The safe point on the game thread where queued writes are applied.</li>
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
