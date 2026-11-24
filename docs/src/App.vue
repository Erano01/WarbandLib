<script setup>
import { computed, ref, watch } from 'vue'
import { useRoute } from 'vue-router'
import { pages } from './router.js'

const route = useRoute()
const logo = import.meta.env.BASE_URL + 'favicon.svg'
const menuOpen = ref(false)
const groups = computed(() => {
  const out = []
  for (const p of pages) {
    let g = out.find((x) => x.name === p.group)
    if (!g) out.push((g = { name: p.group, pages: [] }))
    g.pages.push(p)
  }
  return out
})
const index = computed(() => pages.findIndex((p) => p.path === route.path))
const prev = computed(() => (index.value > 0 ? pages[index.value - 1] : null))
const next = computed(() => (index.value >= 0 && index.value < pages.length - 1 ? pages[index.value + 1] : null))
watch(() => route.path, () => (menuOpen.value = false))
</script>

<template>
  <header class="topbar">
    <button class="menu-toggle" aria-label="Toggle navigation" @click="menuOpen = !menuOpen">☰</button>
    <RouterLink to="/" class="brand">
      <img :src="logo" alt="" width="24" height="24" />
      <span>WarbandLib</span>
    </RouterLink>
    <span class="version">ABI 1.1 · runtime 0.2.0</span>
    <a class="repo" href="https://github.com/Erano01/WarbandLib" target="_blank" rel="noopener">GitHub</a>
  </header>
  <div class="layout">
    <nav class="sidebar" :class="{ open: menuOpen }">
      <div v-for="g in groups" :key="g.name" class="nav-group">
        <div class="nav-title">{{ g.name }}</div>
        <RouterLink v-for="p in g.pages" :key="p.path" :to="p.path" class="nav-link">{{ p.name }}</RouterLink>
      </div>
    </nav>
    <main class="content">
      <RouterView />
      <footer class="pager">
        <RouterLink v-if="prev" :to="prev.path" class="pager-link">← {{ prev.name }}</RouterLink>
        <span v-else />
        <RouterLink v-if="next" :to="next.path" class="pager-link next">{{ next.name }} →</RouterLink>
      </footer>
    </main>
  </div>
</template>
