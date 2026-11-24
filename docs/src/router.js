import { createRouter, createWebHashHistory } from 'vue-router'
import Home from './pages/Home.vue'
import GettingStarted from './pages/GettingStarted.vue'
import Architecture from './pages/Architecture.vue'
import AbiRules from './pages/AbiRules.vue'
import WritingPlugins from './pages/WritingPlugins.vue'
import Reference from './pages/Reference.vue'
import Examples from './pages/Examples.vue'
import Roadmap from './pages/Roadmap.vue'
import GameData from './pages/GameData.vue'
import { privatePages } from 'virtual:private-pages'

export const pages = [
  { path: '/', name: 'Overview', component: Home, group: 'Introduction' },
  { path: '/getting-started', name: 'Getting started', component: GettingStarted, group: 'Introduction' },
  { path: '/architecture', name: 'Architecture', component: Architecture, group: 'Introduction' },
  { path: '/writing-plugins', name: 'Writing a plugin', component: WritingPlugins, group: 'Guides' },
  { path: '/examples', name: 'Example plugins', component: Examples, group: 'Guides' },
  { path: '/abi', name: 'ABI stability rules', component: AbiRules, group: 'Reference' },
  { path: '/reference', name: 'C API reference', component: Reference, group: 'Reference' },
  { path: '/game-data', name: 'Game data model', component: GameData, group: 'Project' },
  { path: '/roadmap', name: 'Roadmap', component: Roadmap, group: 'Project' },
  ...privatePages,
]

export const router = createRouter({
  history: createWebHashHistory(),
  routes: [...pages, { path: '/:rest(.*)*', redirect: '/' }],
  scrollBehavior(to) {
    if (to.hash) return { el: to.hash, top: 72 }
    return { top: 0 }
  },
})
