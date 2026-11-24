#pragma once

#include <string>

#include "runtime/host/symbol_table.h"
#include "warbandlib/warbandlib.h"

namespace warbandlib::runtime::game_events {

// Hooks the game's own dispatch points named in the game data and raises the game events from
// them. Returns how many dispatch points are live; missing data leaves the events silent.
int Install(const host::SymbolTable* symbols);
// Stops raising game events and waits (bounded) until no callback is running any more.
void Quiesce();

bool IsGameThread();
// Called on the game thread between frames (window message loop).
void RunGameThreadTasks();
wbl_result QueueGameThreadTask(const void* owner, wbl_task_fn fn, void* user_data);
void RemoveOwnerTasks(const void* owner);

} // namespace warbandlib::runtime::game_events
