// Project board. Move a card by changing its `status`; keep titles short.
// status: 'done' | 'doing' | 'todo' | 'later'   area: see `areas` below. To do is ordered by priority.
export const columns = [
  { id: 'done', title: 'Done' },
  { id: 'doing', title: 'In progress' },
  { id: 'todo', title: 'To do' },
  { id: 'later', title: 'Later' },
]

export const areas = {
  abi: 'ABI',
  runtime: 'Runtime',
  events: 'Events',
  overlay: 'Overlay',
  data: 'Game data',
  tooling: 'Tooling',
  docs: 'Docs',
  coop: 'Co-op',
}

export const cards = [
  // Done
  { id: 'WBL-1', title: 'Stable C ABI 1.0 with layout lock', area: 'abi', status: 'done' },
  { id: 'WBL-2', title: 'Plugin loader with dependencies and load order', area: 'runtime', status: 'done' },
  { id: 'WBL-3', title: 'Event bus with priorities and custom events', area: 'runtime', status: 'done' },
  { id: 'WBL-4', title: 'Versioned interfaces between plugins', area: 'runtime', status: 'done' },
  { id: 'WBL-5', title: 'Shared priority hook chains', area: 'runtime', status: 'done' },
  { id: 'WBL-6', title: 'Fault-safe memory access', area: 'runtime', status: 'done' },
  { id: 'WBL-7', title: 'Overlay with menu and draw API', area: 'overlay', status: 'done' },
  { id: 'WBL-8', title: 'Clean unload and hot reload', area: 'runtime', status: 'done' },
  { id: 'WBL-9', title: 'Linux (clang-cl + xwin) and Windows builds', area: 'tooling', status: 'done' },
  { id: 'WBL-10', title: 'Example plugins: C, C++, overlay', area: 'docs', status: 'done' },
  { id: 'WBL-11', title: 'Vue documentation site with diagrams', area: 'docs', status: 'done' },
  { id: 'WBL-12', title: 'Private data repo split, history purge', area: 'data', status: 'done' },
  { id: 'WBL-13', title: 'Event system research: battle', area: 'events', status: 'done' },
  { id: 'WBL-14', title: 'Event system research: campaign map and menus', area: 'events', status: 'done' },
  { id: 'WBL-15', title: 'Event system research: towns, dialogs, trade', area: 'events', status: 'done' },
  { id: 'WBL-16', title: 'Thread model of the game measured', area: 'events', status: 'done' },
  { id: 'WBL-17', title: 'Game events page (findings and design)', area: 'docs', status: 'done' },

  // In progress
  { id: 'WBL-18', title: 'Typed event API design (PRE/POST, payloads)', area: 'events', status: 'doing' },

  // To do
  { id: 'WBL-19', title: 'Typed event API in the ABI', area: 'abi', status: 'todo' },
  { id: 'WBL-20', title: 'Mission events: start, end, spawn, hit, killed', area: 'events', status: 'todo' },
  { id: 'WBL-21', title: 'Gated events: weapon attack, missile, shield hit', area: 'events', status: 'todo' },
  { id: 'WBL-22', title: 'Menu and dialog events', area: 'events', status: 'todo' },
  { id: 'WBL-23', title: 'Campaign events: tick, encounter, simulated battle', area: 'events', status: 'todo' },
  { id: 'WBL-24', title: 'Queued writes applied on the game thread', area: 'runtime', status: 'todo' },
  { id: 'WBL-25', title: 'Fix hp_editor data race', area: 'overlay', status: 'todo' },
  { id: 'WBL-26', title: 'Render/game thread sync point research', area: 'events', status: 'todo' },
  { id: 'WBL-27', title: 'Symbol catalogue replacing raw addresses', area: 'data', status: 'todo' },
  { id: 'WBL-28', title: 'Remove addresses from examples, second purge', area: 'data', status: 'todo' },

  // Later
  { id: 'WBL-29', title: 'Event research: sieges and scene props', area: 'events', status: 'later' },
  { id: 'WBL-30', title: 'Event research: multiplayer', area: 'events', status: 'later' },
  { id: 'WBL-31', title: 'Loader without an injector', area: 'tooling', status: 'later' },
  { id: 'WBL-32', title: 'Typed game objects as stable handles', area: 'abi', status: 'later' },
  { id: 'WBL-33', title: 'Content registry (namespaced ids)', area: 'runtime', status: 'later' },
  { id: 'WBL-34', title: 'Module-system bridge: new script operations', area: 'runtime', status: 'later' },
  { id: 'WBL-35', title: 'Public atlas pages', area: 'docs', status: 'later' },
  { id: 'WBL-36', title: 'Campaign co-op plugin set', area: 'coop', status: 'later' },
]
