#include "examples/hp_editor/hp_editor.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "third_party/imgui/imgui.h"

namespace warbandlib::examples::hp_editor {

namespace {

// See re/mb_warband.exe.md: Agent::vftable, and the two fields
// Agent_SetHitPoints (0x0049EE40) writes together.
constexpr std::uintptr_t kAgentVftable = 0x007C4A4C;
constexpr std::ptrdiff_t kHitPointsFloatOffset = 0x6000;
constexpr std::ptrdiff_t kHitPointsIntOffset = 0x6004;

// Regions bigger than this are reserved address space / oversized mappings
// unrelated to the small, densely-packed Agent pool -- skip them so one scan
// can't balloon into a huge allocation.
constexpr SIZE_T kMaxRegionBytesToScan = 128u * 1024u * 1024u;

std::atomic<bool> g_scan_in_progress{false};
std::mutex g_agents_mutex;
std::vector<std::uintptr_t> g_agents;
int g_selected_index = -1;

// Edge-detected key press (true once per physical press, not once per frame
// held down), same idiom as ViewLayer's F10 toggle, generalized to the
// handful of keys this menu binds.
bool KeyPressed(int vk) {
	static std::unordered_map<int, bool> was_down;
	const bool is_down = (GetAsyncKeyState(vk) & 0x8000) != 0;
	bool& prev = was_down[vk];
	const bool edge = is_down && !prev;
	prev = is_down;
	return edge;
}

bool IsReadableCommittedPrivate(const MEMORY_BASIC_INFORMATION& mbi) {
	if (mbi.State != MEM_COMMIT) return false;
	if (mbi.Type != MEM_PRIVATE) return false; // skip loaded module images/mappings
	if ((mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;
	constexpr DWORD kReadableMask = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY;
	return (mbi.Protect & kReadableMask) != 0;
}

// Walks the process's private (heap) memory looking for the Agent::vftable
// pointer sitting at offset 0 of a live object -- the same "vtable pointer
// scan" technique used manually via x32dbg/Cheat Engine, automated. Uses
// ReadProcessMemory on our own process (rather than dereferencing pointers
// directly) because it fails safely on an inaccessible page instead of
// raising a fault -- MinGW builds here have no structured exception handling
// to catch that. Restricted to MEM_PRIVATE (skips the ~100+MB of loaded
// module code/data) so this stays a few tens of ms, not seconds -- this
// previously ran on the render thread and stalling it that long could crash
// the game (e.g. across a pause-menu transition); it now always runs on its
// own worker thread (see StartScan below).
std::vector<std::uintptr_t> ScanForLiveAgents() {
	std::vector<std::uintptr_t> found;
	const HANDLE process = GetCurrentProcess();

	std::uintptr_t address = 0;
	MEMORY_BASIC_INFORMATION mbi{};
	std::vector<std::uint8_t> buffer;

	while (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == sizeof(mbi)) {
		const std::uintptr_t region_base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
		const SIZE_T region_size = mbi.RegionSize;

		if (IsReadableCommittedPrivate(mbi) && region_size >= sizeof(std::uintptr_t) &&
		    region_size <= kMaxRegionBytesToScan) {
			buffer.resize(region_size);
			SIZE_T bytes_read = 0;
			if (ReadProcessMemory(process, mbi.BaseAddress, buffer.data(), buffer.size(), &bytes_read) &&
			    bytes_read >= sizeof(std::uintptr_t)) {
				for (std::size_t offset = 0; offset + sizeof(std::uintptr_t) <= bytes_read;
				     offset += sizeof(std::uintptr_t)) {
					std::uintptr_t candidate = 0;
					std::memcpy(&candidate, buffer.data() + offset, sizeof(candidate));
					if (candidate == kAgentVftable) {
						found.push_back(region_base + offset);
					}
				}
			}
		}

		const std::uintptr_t next = region_base + region_size;
		if (next <= address) break; // guard against a zero-size region stalling the walk
		address = next;
	}

	return found;
}

DWORD WINAPI ScanThreadProc(LPVOID) {
	std::vector<std::uintptr_t> found = ScanForLiveAgents();
	{
		std::lock_guard<std::mutex> lock(g_agents_mutex);
		g_agents = std::move(found);
	}
	g_selected_index = -1;
	g_scan_in_progress.store(false);
	return 0;
}

// Fire-and-forget: runs the (potentially tens-of-ms) address space walk off
// the render thread so it can never stall a frame. A second press while one
// is already running is a no-op.
void StartScan() {
	bool expected = false;
	if (!g_scan_in_progress.compare_exchange_strong(expected, true)) {
		return;
	}
	const HANDLE thread = CreateThread(nullptr, 0, ScanThreadProc, nullptr, 0, nullptr);
	if (thread != nullptr) {
		CloseHandle(thread); // detach -- ScanThreadProc cleans up after itself
	}
}

bool ReadInt32(std::uintptr_t address, std::int32_t& out) {
	SIZE_T bytes_read = 0;
	return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address), &out, sizeof(out),
	                          &bytes_read) &&
	       bytes_read == sizeof(out);
}

// Scenes in this engine are loaded/torn down independently (see
// re/mb_warband.exe.md) -- leaving one frees its Agent pool, which can turn
// an address from a previous scan into memory now used for something else.
// Re-check the vtable pointer is still there before trusting a cached
// address for anything, rather than blindly reading/writing into it.
bool IsLiveAgent(std::uintptr_t address) {
	std::uintptr_t vtable = 0;
	SIZE_T bytes_read = 0;
	return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address), &vtable, sizeof(vtable),
	                          &bytes_read) &&
	       bytes_read == sizeof(vtable) && vtable == kAgentVftable;
}

bool WriteHitPoints(std::uintptr_t agent, std::int32_t hit_points) {
	if (!IsLiveAgent(agent)) {
		return false;
	}
	const float as_float = static_cast<float>(hit_points);
	SIZE_T written = 0;
	const bool wrote_float =
	    WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<LPVOID>(agent + kHitPointsFloatOffset), &as_float,
	                        sizeof(as_float), &written) &&
	    written == sizeof(as_float);
	written = 0;
	const bool wrote_int =
	    WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<LPVOID>(agent + kHitPointsIntOffset), &hit_points,
	                        sizeof(hit_points), &written) &&
	    written == sizeof(hit_points);
	return wrote_float && wrote_int;
}

void AdjustSelectedHitPoints(int delta) {
	std::lock_guard<std::mutex> lock(g_agents_mutex);
	if (g_selected_index < 0 || static_cast<std::size_t>(g_selected_index) >= g_agents.size()) {
		return;
	}
	const std::uintptr_t agent = g_agents[static_cast<std::size_t>(g_selected_index)];
	std::int32_t hp = 0;
	if (ReadInt32(agent + static_cast<std::uintptr_t>(kHitPointsIntOffset), hp)) {
		WriteHitPoints(agent, hp + delta);
	}
}

} // namespace

void DrawMenu() {
	// F11: rescan. Page Up/Down: +-1 hp on the selected agent. Home/End: +-10.
	// Bound to keys (rather than only mouse clicks) because two example
	// menus injected at once fight over the same window's input hook chain
	// and can make precise clicking unreliable -- see re/mb_warband.exe.md
	// and README.md for injecting this example alone.
	if (KeyPressed(VK_F11)) StartScan();
	if (KeyPressed(VK_PRIOR)) AdjustSelectedHitPoints(+1);
	if (KeyPressed(VK_NEXT)) AdjustSelectedHitPoints(-1);
	if (KeyPressed(VK_HOME)) AdjustSelectedHitPoints(+10);
	if (KeyPressed(VK_END)) AdjustSelectedHitPoints(-10);

	std::vector<std::uintptr_t> agents_snapshot;
	{
		std::lock_guard<std::mutex> lock(g_agents_mutex);
		agents_snapshot = g_agents;
	}

	ImGui::Begin("WarbandLib - HP Editor");
	ImGui::TextUnformatted("Agent::hit_points test menu (see re/mb_warband.exe.md)");
	ImGui::TextUnformatted("Keys: F11 rescan, PgUp/PgDn +-1, Home/End +-10");

	if (ImGui::Button("Scan for live agents (F11)")) {
		StartScan();
	}
	ImGui::SameLine();
	if (g_scan_in_progress.load()) {
		ImGui::TextUnformatted("Scanning...");
	} else {
		ImGui::Text("Found: %u", static_cast<unsigned>(agents_snapshot.size()));
	}

	// Drop entries whose scene has since been torn down (see IsLiveAgent)
	// instead of showing/acting on stale addresses.
	std::vector<std::uintptr_t> still_live;
	still_live.reserve(agents_snapshot.size());
	for (std::uintptr_t candidate : agents_snapshot) {
		if (IsLiveAgent(candidate)) {
			still_live.push_back(candidate);
		}
	}
	if (still_live.size() != agents_snapshot.size()) {
		std::lock_guard<std::mutex> lock(g_agents_mutex);
		g_agents = still_live;
		g_selected_index = -1;
	}
	agents_snapshot = still_live;

	for (std::size_t i = 0; i < agents_snapshot.size(); ++i) {
		std::int32_t hp = 0;
		ReadInt32(agents_snapshot[i] + static_cast<std::uintptr_t>(kHitPointsIntOffset), hp);
		char label[64];
		std::snprintf(label, sizeof(label), "0x%08X  hit_points=%d", static_cast<unsigned>(agents_snapshot[i]), hp);
		if (ImGui::Selectable(label, g_selected_index == static_cast<int>(i))) {
			g_selected_index = static_cast<int>(i);
		}
	}

	if (g_selected_index >= 0 && static_cast<std::size_t>(g_selected_index) < agents_snapshot.size()) {
		const std::uintptr_t agent = agents_snapshot[static_cast<std::size_t>(g_selected_index)];
		std::int32_t hp = 0;
		ReadInt32(agent + static_cast<std::uintptr_t>(kHitPointsIntOffset), hp);

		ImGui::Separator();
		ImGui::Text("Selected: 0x%08X", static_cast<unsigned>(agent));
		ImGui::Text("hit_points: %d", hp);
		if (ImGui::Button("-10")) WriteHitPoints(agent, hp - 10);
		ImGui::SameLine();
		if (ImGui::Button("-1")) WriteHitPoints(agent, hp - 1);
		ImGui::SameLine();
		if (ImGui::Button("+1")) WriteHitPoints(agent, hp + 1);
		ImGui::SameLine();
		if (ImGui::Button("+10")) WriteHitPoints(agent, hp + 10);
	}

	ImGui::End();
}

} // namespace warbandlib::examples::hp_editor
