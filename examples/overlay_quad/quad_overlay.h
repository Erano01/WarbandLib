#pragma once

#include "core/logging.h"

namespace warbandlib::examples::overlay_quad {

// Optional: routes diagnostic lines (visibility state, F9 key reads) to the
// given logger. Call once before the hook installs; pass nullptr to
// silence diagnostics.
void SetLogger(core::Logger* logger);

// EndSceneHook::TickCallback-compatible entry point: draws a small
// untextured colored quad in the top-left corner as a "mod is alive"
// indicator, and toggles visibility on an F9 key edge. Safe by
// construction: captures the full device state with CreateStateBlock
// before touching anything and restores it via Apply() before returning,
// so the game's own next draw calls see exactly the state they left.
// Also resets the viewport to the full backbuffer before drawing, so the
// quad isn't clipped by whatever sub-region viewport the game last used.
void Tick(void* device);

} // namespace warbandlib::examples::overlay_quad
