#include "examples/imgui_menu/example_menu.h"

#include "third_party/imgui/imgui.h"

namespace warbandlib::examples::imgui_menu {

namespace {
bool g_example_checkbox = false;
int g_click_count = 0;
} // namespace

void DrawMenu() {
	ImGui::Begin("WarbandLib");
	ImGui::Text("Framerate: %.1f", static_cast<double>(ImGui::GetIO().Framerate));
	ImGui::Checkbox("Example checkbox", &g_example_checkbox);
	if (ImGui::Button("Click me")) {
		++g_click_count;
	}
	ImGui::Text("Clicks: %d", g_click_count);
	ImGui::End();
}

} // namespace warbandlib::examples::imgui_menu
