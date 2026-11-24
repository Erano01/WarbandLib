<template>
  <h1>Example plugins</h1>
  <p class="lead">Four plugins in <code>examples/</code>, built into <code>bin/plugins/</code>, that together exercise the whole ABI.</p>

  <h2><code>example.hello_c</code> — plain C</h2>
  <p>Proves the header is real C. Draws a banner every frame with the draw api, shows a menu window with a checkbox and a button, registers the custom event <code>example.hello_c.greeted</code> and posts it on click, publishes the interface <code>example.hello_c.api</code> v1, and hooks <code>Agent_SetHitPoints</code> at <code>WBL_PRIORITY_HIGH</code>, counting calls.</p>

  <h2><code>example.hp_editor</code> — C++</h2>
  <p>Declares an optional dependency on <code>example.hello_c</code>, so it loads after it, and reads that plugin's greet counter through its interface. Scans the heap for live agents on a worker thread with the memory api, lists their hit points, and changes them by calling the game's own <code>Agent_SetHitPoints</code>. The click happens on the render thread, so the change is queued with <code>run_on_game_thread</code> and applied on the game thread between frames. It hooks the same function at <code>WBL_PRIORITY_NORMAL</code>, so a click goes through both plugins' detours before reaching the game: a live two-plugin hook chain.</p>

  <h2><code>example.game_events</code> — game events, plain C</h2>
  <p>Subscribes to the POST phase of every game event and contains no game address at all. Its menu window counts module triggers by source, engine script calls (with the last script's name), dialog and menu blocks, and agent hits with the last victim, attacker and damage. Two switches show the writable payloads: <em>No damage to anyone</em> sets <code>damage</code> to 0 on every hit, <em>Hide every dialog line</em> sets <code>passed</code> to false on dialog conditions. The first event of each kind is written to the log together with whether it arrived on the game thread.</p>

  <h2><code>example.overlay_quad</code> — raw Direct3D 9</h2>
  <p>Subscribes to <code>WBL_EVENT_FRAME</code> and draws with the device directly, inside a state block so the game's render state is untouched. F9 toggles it.</p>
</template>
