import { existsSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

const repoRoot = fileURLToPath(new URL('..', import.meta.url))
const privateIndex = fileURLToPath(new URL('../internal/docs/index.js', import.meta.url))
const privateStub = fileURLToPath(new URL('./src/private-stub.js', import.meta.url))

// Relative base + hash routing: the built site works from any folder or GitHub Pages path.
// Private pages come from the internal/ submodule and are wired in for the dev server only,
// so they never reach dist/; without the submodule the stub keeps the site working.
export default defineConfig(({ command }) => ({
  base: './',
  plugins: [vue()],
  resolve: {
    alias: {
      '@docs': fileURLToPath(new URL('./src', import.meta.url)),
      'virtual:private-pages': command === 'serve' && existsSync(privateIndex) ? privateIndex : privateStub,
    },
    dedupe: ['vue', 'vue-router', 'highlight.js'],
  },
  server: {
    fs: { allow: [repoRoot] },
  },
}))
