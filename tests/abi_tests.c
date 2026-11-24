/* Compiled as C: the public header must stay valid C. */
#include "warbandlib/warbandlib.h"

int wbl_test_c_has_member(uint32_t struct_size) {
	wbl_core_api api = {0};
	api.struct_size = struct_size;
	return WBL_HAS_MEMBER(&api, wbl_core_api, get_interface) ? 1 : 0;
}

uint32_t wbl_test_c_rgba(void) { return WBL_RGBA(0x11, 0x22, 0x33, 0x44); }
