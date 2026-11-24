import { existsSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

const privateIndex = fileURLToPath(new URL('./src/private/index.js', import.meta.url))
const privateStub = fileURLToPath(new URL('./src/private-stub.js', import.meta.url))

// Relative base + hash routing: the built site works from any folder or GitHub Pages path.
// src/private/ is gitignored and only wired in for the dev server, so it can never reach dist/.
export default defineConfig(({ command }) => ({
  base: './',
  plugins: [vue()],
  resolve: {
    alias: {
      'virtual:private-pages': command === 'serve' && existsSync(privateIndex) ? privateIndex : privateStub,
    },
  },
}))
