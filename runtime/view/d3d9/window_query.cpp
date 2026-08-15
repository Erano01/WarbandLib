#include "runtime/view/d3d9/window_query.h"

#include <windows.h>
#include <d3d9.h>

namespace warbandlib::runtime::view::d3d9 {

void* ResolveFocusWindow(void* device_ptr) {
	auto* device = static_cast<IDirect3DDevice9*>(device_ptr);

	D3DDEVICE_CREATION_PARAMETERS params{};
	if (FAILED(device->GetCreationParameters(&params))) {
		return nullptr;
	}
	return params.hFocusWindow;
}

} // namespace warbandlib::runtime::view::d3d9
