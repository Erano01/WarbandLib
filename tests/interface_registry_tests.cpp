#include <gtest/gtest.h>

#include "runtime/host/interface_registry.h"

using warbandlib::host::InterfaceRegistry;

TEST(InterfaceRegistry, VersionGateAndOwnership) {
	InterfaceRegistry reg;
	int a = 0, b = 0;
	const int table_v2 = 2, table_v3 = 3;
	EXPECT_EQ(reg.Register(&a, "coop.net", 2, &table_v2), WBL_OK);
	EXPECT_EQ(reg.Register(&b, "coop.net", 5, &table_v3), WBL_ERR_ALREADY_EXISTS);
	std::uint32_t version = 0;
	EXPECT_EQ(reg.Get("coop.net", 1, &version), &table_v2);
	EXPECT_EQ(version, 2u);
	EXPECT_EQ(reg.Get("coop.net", 3, nullptr), nullptr);
	EXPECT_EQ(reg.Register(&a, "coop.net", 3, &table_v3), WBL_OK);
	EXPECT_EQ(reg.Get("coop.net", 3, nullptr), &table_v3);
	reg.RemoveOwner(&a);
	EXPECT_EQ(reg.Get("coop.net", 0, nullptr), nullptr);
	EXPECT_EQ(reg.Register(&b, "", 1, &table_v2), WBL_ERR_INVALID_ARGUMENT);
}
