<script setup>
defineProps({ kind: { type: String, default: 'note' }, title: String })
</script>

<template>
  <aside class="callout" :class="kind">
    <strong v-if="title">{{ title }}</strong>
    <slot />
  </aside>
</template>

<style scoped>
.callout { margin: 18px 0; padding: 12px 16px; border-radius: 8px; border-left: 4px solid var(--accent); background: var(--accent-soft); }
.callout.warn { border-color: var(--warn); background: color-mix(in srgb, var(--warn) 12%, transparent); }
.callout.ok { border-color: var(--ok); background: color-mix(in srgb, var(--ok) 12%, transparent); }
strong { display: block; margin-bottom: 4px; }
.callout :deep(p) { margin: 4px 0; }
</style>
