// C++ plugin: finds live agents, shows and changes their hit points through the
// game's own setter, and uses example.hello_c's interface when it is present.
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <vector>

#include "warbandlib/warbandlib.h"

namespace {

constexpr std::uintptr_t kAgentVftableVa = 0x007C4A4C;
constexpr std::uintptr_t kAgentSetHitPointsVa = 0x0049EE40;
constexpr std::uintptr_t kHitPointsIntOffset = 0x6004;
constexpr SIZE_T kMaxRegionBytes = 128u * 1024u * 1024u;

struct HelloApiV1 {
	std::uint32_t struct_size;
	std::uint32_t(WBL_CALL* greet_count)();
};

// __thiscall(Agent* this, float* hp) seen through __fastcall: this in ECX, EDX unused.
using SetHitPointsFn = void(__fastcall*)(void* agent, void* edx, float* hp);

const wbl_host_api* g_host = nullptr;
std::uintptr_t g_agent_vftable = 0;
SetHitPointsFn g_set_hit_points = nullptr;
SetHitPointsFn g_original_set_hit_points = nullptr;
const HelloApiV1* g_hello = nullptr;

std::atomic<bool> g_scanning{false};
std::mutex g_mutex;
std::vector<std::uintptr_t> g_agents;
int g_selected = -1;
std::atomic<std::uint32_t> g_hp_calls{0};
float g_last_hp = 0.0f;

void __fastcall SetHitPointsDetour(void* agent, void* edx, float* hp) {
	++g_hp_calls;
	if (hp != nullptr) g_last_hp = *hp;
	g_original_set_hit_points(agent, edx, hp);
}

bool Readable(const MEMORY_BASIC_INFORMATION& m) {
	return m.State == MEM_COMMIT && m.Type == MEM_PRIVATE && (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0 &&
	       (m.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY)) != 0;
}

DWORD WINAPI ScanThread(LPVOID) {
	std::vector<std::uintptr_t> found;
	std::vector<std::uint32_t> buffer;
	std::uintptr_t address = 0;
	MEMORY_BASIC_INFORMATION mbi{};
	while (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == sizeof(mbi)) {
		const auto base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
		if (Readable(mbi) && mbi.RegionSize <= kMaxRegionBytes) {
			buffer.resize(mbi.RegionSize / 4);
			if (g_host->memory->read(base, buffer.data(), static_cast<std::uint32_t>(buffer.size() * 4)) == WBL_OK) {
				for (std::size_t i = 0; i < buffer.size(); ++i) {
					if (buffer[i] == g_agent_vftable) found.push_back(base + i * 4);
				}
			}
		}
		const std::uintptr_t next = base + mbi.RegionSize;
		if (next <= address) break;
		address = next;
	}
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		g_agents = std::move(found);
		g_selected = -1;
	}
	g_scanning = false;
	return 0;
}

void StartScan() {
	bool expected = false;
	if (!g_scanning.compare_exchange_strong(expected, true)) return;
	const HANDLE t = CreateThread(nullptr, 0, ScanThread, nullptr, 0, nullptr);
	if (t != nullptr) {
		CloseHandle(t);
	} else {
		g_scanning = false;
	}
}

bool IsLiveAgent(std::uintptr_t agent) {
	std::uint32_t vftable = 0;
	return g_host->memory->read(agent, &vftable, sizeof(vftable)) == WBL_OK && vftable == g_agent_vftable;
}

void SetHitPoints(std::uintptr_t agent, int hp) {
	if (!IsLiveAgent(agent)) return;
	float value = static_cast<float>(hp < 0 ? 0 : hp);
	g_set_hit_points(reinterpret_cast<void*>(agent), nullptr, &value);
}

wbl_event_reply WBL_CALL OnUi(const wbl_event*, void*) {
	const wbl_ui_api* ui = g_host->ui;
	char line[128];
	if (ui->begin_window("hp_editor", nullptr)) {
		if (ui->button(g_scanning ? "Scanning..." : "Scan for agents")) StartScan();
		std::vector<std::uintptr_t> agents;
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			agents = g_agents;
		}
		ui->same_line();
		std::snprintf(line, sizeof(line), "found: %u", static_cast<unsigned>(agents.size()));
		ui->text(line);
		std::snprintf(line, sizeof(line), "Agent_SetHitPoints calls: %u (last %.1f)", g_hp_calls.load(),
		              static_cast<double>(g_last_hp));
		ui->text(line);
		if (g_hello != nullptr) {
			std::snprintf(line, sizeof(line), "hello_c greets (via interface): %u", g_hello->greet_count());
			ui->text(line);
		}
		ui->separator();
		for (std::size_t i = 0; i < agents.size(); ++i) {
			std::int32_t hp = 0;
			if (!IsLiveAgent(agents[i]) || g_host->memory->read(agents[i] + kHitPointsIntOffset, &hp, 4) != WBL_OK) continue;
			std::snprintf(line, sizeof(line), "#%u  0x%08X  hp %d", static_cast<unsigned>(i),
			              static_cast<unsigned>(agents[i]), hp);
			if (ui->selectable(line, g_selected == static_cast<int>(i))) g_selected = static_cast<int>(i);
		}
		if (g_selected >= 0 && static_cast<std::size_t>(g_selected) < agents.size()) {
			const std::uintptr_t agent = agents[static_cast<std::size_t>(g_selected)];
			std::int32_t hp = 0;
			g_host->memory->read(agent + kHitPointsIntOffset, &hp, 4);
			ui->separator();
			const int deltas[] = {-10, -1, 1, 10};
			const char* labels[] = {"-10", "-1", "+1", "+10"};
			for (int k = 0; k < 4; ++k) {
				if (k > 0) ui->same_line();
				if (ui->button(labels[k])) SetHitPoints(agent, hp + deltas[k]);
			}
		}
	}
	ui->end_window();
	return WBL_EVENT_CONTINUE;
}

wbl_event_reply WBL_CALL OnPluginsLoaded(const wbl_event*, void*) {
	std::uint32_t version = 0;
	g_hello = static_cast<const HelloApiV1*>(g_host->core->get_interface("example.hello_c.api", 1, &version));
	return WBL_EVENT_CONTINUE;
}

const wbl_dependency kDependencies[] = {
    {sizeof(wbl_dependency), "example.hello_c", "1.0.0", WBL_DEPENDENCY_OPTIONAL},
};

} // namespace

WBL_PLUGIN_QUERY {
	static const wbl_plugin_info info = {
	    sizeof(wbl_plugin_info), WBL_ABI_VERSION_MAJOR, 0, "example.hp_editor", "HP editor", "1.0.0", "WarbandLib",
	    "Lists live agents and edits their hit points through Agent_SetHitPoints.", 1, kDependencies,
	};
	return &info;
}

WBL_PLUGIN_LOAD(host, self) {
	g_host = host;
	g_agent_vftable = host->core->rebase(kAgentVftableVa);
	g_set_hit_points = reinterpret_cast<SetHitPointsFn>(host->core->rebase(kAgentSetHitPointsVa));

	const wbl_result hooked = host->hooks->create(self, reinterpret_cast<void*>(g_set_hit_points),
	                                              reinterpret_cast<void*>(&SetHitPointsDetour), WBL_PRIORITY_NORMAL,
	                                              reinterpret_cast<void**>(&g_original_set_hit_points), nullptr);
	char line[96];
	std::snprintf(line, sizeof(line), "Agent_SetHitPoints hook: %d", hooked);
	host->core->log(self, WBL_LOG_INFO, line);

	host->events->subscribe(self, WBL_EVENT_UI, WBL_PRIORITY_NORMAL, OnUi, nullptr, nullptr);
	host->events->subscribe(self, WBL_EVENT_PLUGINS_LOADED, WBL_PRIORITY_NORMAL, OnPluginsLoaded, nullptr, nullptr);
	return WBL_OK;
}

WBL_PLUGIN_UNLOAD {}
