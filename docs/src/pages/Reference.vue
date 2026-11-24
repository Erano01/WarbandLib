<script setup>
import { tables, events, results } from '../data/api.js'
</script>

<template>
  <h1>C API reference</h1>
  <p class="lead">Everything a plugin can reach, from <code>sdk/include/warbandlib/warbandlib.h</code>. All functions use <code>__cdecl</code>; strings are UTF-8.</p>

  <nav class="toc">
    <a v-for="t in tables" :key="t.id" :href="'#/reference#' + t.id">{{ t.name }}</a>
    <a href="#/reference#events">events</a>
    <a href="#/reference#results">result codes</a>
  </nav>

  <section v-for="t in tables" :id="t.id" :key="t.id">
    <h2><code>{{ t.name }}</code></h2>
    <p>{{ t.summary }}</p>
    <table>
      <thead><tr><th>Member</th><th>Meaning</th></tr></thead>
      <tbody>
        <tr v-for="m in t.members" :key="m[0]">
          <td><code class="sig">{{ m[0] }}</code></td>
          <td>{{ m[1] }}</td>
        </tr>
      </tbody>
    </table>
  </section>

  <section id="events">
    <h2>Built-in events</h2>
    <table>
      <thead><tr><th>Id</th><th>Value</th><th>data</th><th>When</th></tr></thead>
      <tbody>
        <tr v-for="e in events" :key="e[0]"><td><code>{{ e[0] }}</code></td><td>{{ e[1] }}</td><td><code>{{ e[2] }}</code></td><td>{{ e[3] }}</td></tr>
      </tbody>
    </table>
    <p>Callbacks return <code>WBL_EVENT_CONTINUE</code> or <code>WBL_EVENT_CANCEL</code>; cancelling only stops events registered with <code>WBL_EVENT_FLAG_CANCELLABLE</code>.</p>
  </section>

  <section id="results">
    <h2>Result codes</h2>
    <table>
      <thead><tr><th>Name</th><th>Value</th></tr></thead>
      <tbody><tr v-for="r in results" :key="r[0]"><td><code>{{ r[0] }}</code></td><td>{{ r[1] }}</td></tr></tbody>
    </table>
  </section>
</template>

<style scoped>
.toc { display: flex; flex-wrap: wrap; gap: 8px; margin: 8px 0 8px; }
.toc a { font: 13px var(--mono); padding: 3px 9px; border: 1px solid var(--border); border-radius: 20px; background: var(--surface); }
.sig { white-space: pre-wrap; word-break: break-word; background: none; padding: 0; }
td:first-child { width: 55%; }
</style>
