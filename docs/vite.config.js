import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// Relative base + hash routing: the built site works from any folder or GitHub Pages path.
export default defineConfig({
  base: './',
  plugins: [vue()],
})
