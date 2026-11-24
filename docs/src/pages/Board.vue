<script setup>
import { computed, ref } from 'vue'
import { columns, areas, cards } from '../data/board.js'

const area = ref('all')
const query = ref('')

const visible = computed(() => {
  const q = query.value.trim().toLowerCase()
  return cards.filter(c =>
    (area.value === 'all' || c.area === area.value) &&
    (!q || c.title.toLowerCase().includes(q) || c.id.toLowerCase().includes(q)))
})
const byColumn = computed(() => Object.fromEntries(columns.map(col => [col.id, visible.value.filter(c => c.status === col.id)])))
const progress = computed(() => {
  const planned = cards.filter(c => c.status !== 'later')
  return { done: planned.filter(c => c.status === 'done').length, total: planned.length }
})
</script>

<template>
  <h1>Board</h1>
  <p class="lead">What we have done, what we are doing, what we will do next and what comes later. Cards live in <code>docs/src/data/board.js</code>: moving a card means changing its status there.</p>

  <div class="bar">
    <div class="progress" :title="`${progress.done} of ${progress.total} cards done`">
      <span :style="{ width: (100 * progress.done / progress.total) + '%' }"></span>
    </div>
    <small>{{ progress.done }} / {{ progress.total }} done (Later not counted)</small>
  </div>

  <div class="filters">
    <input v-model="query" type="search" placeholder="Search cards" aria-label="Search cards" />
    <button :class="{ on: area === 'all' }" @click="area = 'all'">All</button>
    <button v-for="(label, id) in areas" :key="id" :class="['area-' + id, { on: area === id }]" @click="area = id">{{ label }}</button>
  </div>

  <div class="board">
    <section v-for="col in columns" :key="col.id" class="column" :class="col.id">
      <header>{{ col.title }} <span class="count">{{ byColumn[col.id].length }}</span></header>
      <article v-for="c in byColumn[col.id]" :key="c.id" class="card">
        <div class="title">{{ c.title }}</div>
        <div class="meta">
          <span class="key">{{ c.id }}</span>
          <span class="tag" :class="'area-' + c.area">{{ areas[c.area] }}</span>
        </div>
      </article>
      <p v-if="!byColumn[col.id].length" class="empty">No cards</p>
    </section>
  </div>
</template>

<style scoped>
.bar { display: flex; align-items: center; gap: 12px; margin: 8px 0 16px; }
.progress { flex: 1; max-width: 360px; height: 8px; border-radius: 4px; background: var(--border); overflow: hidden; }
.progress span { display: block; height: 100%; background: var(--ok); }
.filters { display: flex; flex-wrap: wrap; gap: 6px; margin-bottom: 16px; }
.filters input { padding: 5px 10px; border: 1px solid var(--border); border-radius: 6px; background: transparent; color: inherit; font: inherit; min-width: 180px; }
.filters button { padding: 4px 10px; border: 1px solid var(--border); border-radius: 14px; background: transparent; color: inherit; font: 13px inherit; cursor: pointer; }
.filters button.on { background: var(--accent-soft); border-color: var(--accent); }
.board { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 12px; align-items: start; }
@media (max-width: 1000px) { .board { grid-template-columns: repeat(2, minmax(0, 1fr)); } }
@media (max-width: 560px) { .board { grid-template-columns: 1fr; } }
.column { border: 1px solid var(--border); border-radius: 8px; padding: 8px; background: var(--surface-2); }
.column header { font-weight: 600; font-size: 13px; text-transform: uppercase; letter-spacing: .04em; padding: 4px 4px 8px; display: flex; justify-content: space-between; }
.column.doing header { color: var(--accent); }
.column.done header { color: var(--ok); }
.count { font: 12px var(--mono); color: var(--muted); }
.card { border: 1px solid var(--border); border-radius: 6px; padding: 8px 10px; margin-bottom: 8px; background: var(--surface); }
.column.done .card .title { color: var(--muted); }
.title { font-size: 14px; line-height: 1.35; }
.meta { display: flex; justify-content: space-between; align-items: center; margin-top: 6px; }
.key { font: 11px var(--mono); color: var(--muted); }
.tag { font-size: 11px; padding: 1px 7px; border-radius: 10px; border: 1px solid var(--border); }
.empty { color: var(--muted); font-size: 13px; text-align: center; margin: 8px 0; }
.area-abi { border-color: #8b5cf6; }
.area-runtime { border-color: #3b82f6; }
.area-events { border-color: #f59e0b; }
.area-overlay { border-color: #ec4899; }
.area-data { border-color: #14b8a6; }
.area-tooling { border-color: #64748b; }
.area-docs { border-color: #22c55e; }
.area-coop { border-color: #ef4444; }
</style>
