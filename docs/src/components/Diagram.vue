<script>
// mermaid.render is not re-entrant: every diagram on the page renders through one shared queue.
let queue = Promise.resolve()
let seq = 0
function enqueue(job) {
  queue = queue.then(job, job)
  return queue
}
function nextId() {
  return `wbl-diagram-${seq++}`
}
</script>

<script setup>
import { onMounted, onBeforeUnmount, ref, watch } from 'vue'

const props = defineProps({ code: { type: String, required: true }, caption: String })
const host = ref(null)
const error = ref('')
const media = window.matchMedia('(prefers-color-scheme: dark)')

function render() {
  return enqueue(renderNow)
}

async function renderNow() {
  try {
    const { default: mermaid } = await import('mermaid')
    mermaid.initialize({
      startOnLoad: false,
      theme: media.matches ? 'dark' : 'neutral',
      securityLevel: 'strict',
      fontFamily: 'Inter, system-ui, sans-serif',
    })
    const { svg } = await mermaid.render(nextId(), props.code.trim())
    if (host.value) host.value.innerHTML = svg
    error.value = ''
  } catch (e) {
    error.value = String(e?.message || e)
  }
}

onMounted(() => {
  render()
  media.addEventListener('change', render)
})
onBeforeUnmount(() => media.removeEventListener('change', render))
watch(() => props.code, render)
</script>

<template>
  <figure class="diagram">
    <div ref="host" class="canvas" />
    <pre v-if="error" class="error">{{ error }}</pre>
    <figcaption v-if="caption">{{ caption }}</figcaption>
  </figure>
</template>

<style scoped>
.diagram { margin: 20px 0; padding: 16px; border: 1px solid var(--border); border-radius: 10px; background: var(--surface); }
.canvas { display: flex; justify-content: center; overflow-x: auto; }
.canvas :deep(svg) { max-width: 100%; height: auto; }
figcaption { margin-top: 10px; font-size: 14px; color: var(--muted); text-align: center; }
.error { color: var(--warn); white-space: pre-wrap; font-size: 13px; }
</style>
