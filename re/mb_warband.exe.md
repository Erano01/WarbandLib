# mb_warband.exe — recovered addresses

Fingerprint (SHA-256): `ff4a28c51bed49fe4bbb4bb96389f7611894f6d031bf68fff661efdc946dae13`
Image base at analysis time: `0x00400000` (non-ASLR)

| Name | Address | Type |
|---|---|---|
| `CD3DApplication_Initialize3DEnvironment` | `0x00402230` | function |
| `CD3DApplication_HandleDeviceLost` | `0x00402790` | function |
| `CMyD3DApplication::vftable` | `0x00816A14` | vtable |
| `g_pMyD3DApplication` | `0x02DAB810` | global (`CMyD3DApplication` instance) |
| `g_pMyD3DApplication + 0xC` | `0x02DAB81C` | field (`IDirect3DDevice9*`) |
| `Agent::vftable` | `0x007C4A4C` | vtable |
| `Agent::vftable[0]` (likely scalar deleting dtor) | `0x004562E0` | function |
| `Agent::vftable[1]`, `Agent::vftable[2]` | `0x00651A80` | function (shared by both slots) |
| `Agent_SetHitPoints` (renamed) | `0x0049EE40` | function |
| `Agent + 0x6000` | — | field (`float hit_points`) |
| `Agent + 0x6004` | — | field (`int hit_points`, rounded — HUD/logic-facing value) |

`IDirect3DDevice9` vtable: `EndScene` = index 42 (offset `0xA8`), `Present` = index 17 (offset `0x44`), `Reset` = index 16 (offset `0x40`), `GetCreationParameters` = index 9 (offset `0x24`).

`Agent` RTTI-confirmed (`.?AVAgent@@`, type descriptor `0x0085F63C`), resolved via `RTTI_Complete_Object_Locator` at `0x0084C9AC`. `hit_points` offset found and confirmed (2026-08-15, see below) — two earlier static leads (`FUN_00492280` referencing string `"hit_points_int"`, `FUN_00549C10` referencing `"ui_hit_points"`) had turned out to index unrelated structs (multiplayer state-sync diff buffer and `Party` troop-roster tooltip data respectively), not the live `Agent` instance; the real field was only found dynamically.

**`hit_points` — confirmed.** User ran Cheat Engine (`/opt/cheat-engine-zh`, launched via `wine` under the game's own Proton prefix so it can see the process — the desktop launcher's default `~/.wine` prefix can't) directly against the running game: exact-value scan narrowed with repeated "decreased value by 5" scans (self-inflicted fall damage) down to one address, then "find out what writes to this address" caught the write instruction:
```
0049EE7A - 89 86 04600000 - mov [esi+00006004],eax
```
Decompiling the containing function (`0x0049EE40`, renamed `Agent_SetHitPoints`) confirms both fields in one setter:
```c
void __thiscall Agent_SetHitPoints(Agent *this, float *newHP)
{
  this->field_0x6000 = *newHP;               // float hit_points
  this->field_0x6004 = round_to_int(*newHP);  // int hit_points (HUD/logic-facing)
}
```
`0x6004 < 0x6200` (confirmed object size from the pool scan below) — consistent, near the tail of the object alongside other per-instance runtime state. Screenshot of the CE session in `docs/mb_warband_hitpoints_ce.png`: `esi=0x375C4EE0` (Agent `this`), write target `0x375CAEE4` = `this + 0x6004`, `eax=0x29` (41 decimal) matching the displayed value.

Dynamic session (2026-08-15, x32dbg attached under the game's own Proton prefix, PID resolved via `wine tasklist` since the Linux PID differs from the Windows PID under Wine):
- Live `Agent` instances found via vtable-pointer scan (`memory_search` for bytes `4C 4A 7C 00` = `Agent::vftable` address `0x007C4A4C`). Confirms a fixed-size **pool of 16 concurrent Agent slots**, each object exactly **`0x6202` bytes apart** (object size ≈ `0x6200`/25088 bytes), contiguous in memory. Field at object offset `+8` is a slot index (0-15); inactive slots are all-zero past the vtable ptr + index.
- Destructor chain (from `Agent::vftable[0]`, `0x004562E0`): `FUN_004562e0` (scalar-deleting wrapper) → `FUN_00456160` → calls `FUN_00455fd0` then frees `*(this+0x5a20)`. `FUN_00455fd0` releases a ref-counted array member (count at `+0x14`, data ptr at `+0x10`, refcount pair at `+0x18`/`+0x1c`). None of these offsets are `hit_points` (they're generic base-class/array-owner fields), but they confirm real struct layout near the front and tail of the object.
- Tried before/after byte-diffing the first 512 bytes of all active pool slots across a controlled HP loss (arena death, then repeated fall-damage in a castle scene, confirmed "5 damage taken" by the user). Inconclusive: in the castle scene 9/16 slots showed heavy churn (position/orientation floats, consistent with actively-simulated NPCs on patrol) but none showed a clean int/float delta of `-5` in the first 512 bytes; 7/16 slots were byte-identical before/after (likely non-simulated/culled agents). Never conclusively identified which pool slot is the player (checked `g_pMyD3DApplication` for a "controlled agent" pointer field in its first 0x800 bytes — no match; a broad `memory_search` for candidate pool addresses as raw bytes was too noisy, those bit patterns collide with ordinary small floats).
- Conclusion: `hit_points` is likely past the first 512 bytes of the ~25KB object, and reliable dynamic capture needs either (a) confirming the player's specific slot first (e.g. a hardware/memory write breakpoint on a *narrow* candidate offset once one is guessed, or catching the camera/controller's "active agent" pointer some other way), or (b) a **memory write breakpoint** (`breakpoint_set` with `type=memory`) on a candidate sub-range at the moment of a controlled, isolated HP change — full-object breakpoints are too noisy since moving agents write to themselves every frame (position/animation), so the range must be narrowed first or the target must be standing completely still.
