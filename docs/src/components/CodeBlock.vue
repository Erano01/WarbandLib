<script setup>
import { computed, ref } from 'vue'
import hljs from 'highlight.js/lib/core'
import c from 'highlight.js/lib/languages/c'
import cpp from 'highlight.js/lib/languages/cpp'
import bash from 'highlight.js/lib/languages/bash'
import cmake from 'highlight.js/lib/languages/cmake'
import 'highlight.js/styles/github-dark.css'

hljs.registerLanguage('c', c)
hljs.registerLanguage('cpp', cpp)
hljs.registerLanguage('bash', bash)
hljs.registerLanguage('cmake', cmake)

const props = defineProps({ code: { type: String, required: true }, lang: { type: String, default: 'c' }, title: String })
const html = computed(() => hljs.highlight(props.code.trim(), { language: props.lang }).value)
const copied = ref(false)
async function copy() {
  try {
    await navigator.clipboard.writeText(props.code.trim())
    copied.value = true
    setTimeout(() => (copied.value = false), 1200)
  } catch { /* clipboard unavailable */ }
}
</script>

<template>
  <figure class="code">
    <figcaption>
      <span>{{ title || lang }}</span>
      <button @click="copy">{{ copied ? 'Copied' : 'Copy' }}</button>
    </figcaption>
    <pre><code class="hljs" v-html="html" /></pre>
  </figure>
</template>

<style scoped>
.code { margin: 18px 0; border-radius: 10px; overflow: hidden; background: var(--code-bg); border: 1px solid var(--border); }
figcaption {
  display: flex; justify-content: space-between; align-items: center; padding: 6px 14px;
  font: 12px var(--mono); color: #b9ada1; background: rgba(255,255,255,.04); border-bottom: 1px solid rgba(255,255,255,.06);
}
button { background: none; border: 1px solid rgba(255,255,255,.15); color: #d8cdc2; border-radius: 5px; font-size: 12px; padding: 2px 8px; cursor: pointer; }
pre { margin: 0; padding: 14px 16px; overflow-x: auto; }
code.hljs { background: none; padding: 0; font: 13.5px/1.6 var(--mono); color: var(--code-text); }
</style>
