#include <gtest/gtest.h>

#include "runtime/host/semver.h"
#include "warbandlib/warbandlib.h"

using warbandlib::host::CompareVersions;
using warbandlib::host::ParseVersion;

extern "C" int wbl_test_c_has_member(std::uint32_t struct_size);
extern "C" std::uint32_t wbl_test_c_rgba(void);

TEST(Semver, ParsesPartialAndSuffixed) {
	const auto a = ParseVersion("1.2.3-beta+7");
	ASSERT_TRUE(a);
	EXPECT_EQ(a->major, 1u);
	EXPECT_EQ(a->minor, 2u);
	EXPECT_EQ(a->patch, 3u);
	const auto b = ParseVersion("2");
	ASSERT_TRUE(b);
	EXPECT_EQ(b->minor, 0u);
}

TEST(Semver, RejectsMalformed) {
	EXPECT_FALSE(ParseVersion(""));
	EXPECT_FALSE(ParseVersion("1..2"));
	EXPECT_FALSE(ParseVersion("1.2.3.4"));
	EXPECT_FALSE(ParseVersion("v1"));
	EXPECT_FALSE(ParseVersion("1."));
	EXPECT_FALSE(ParseVersion("99999999999"));
}

TEST(Semver, Orders) {
	EXPECT_LT(CompareVersions(*ParseVersion("1.2.3"), *ParseVersion("1.10.0")), 0);
	EXPECT_EQ(CompareVersions(*ParseVersion("1.2"), *ParseVersion("1.2.0")), 0);
	EXPECT_GT(CompareVersions(*ParseVersion("2.0.0"), *ParseVersion("1.99.99")), 0);
}

TEST(Abi, HeaderIsValidC) {
	EXPECT_EQ(wbl_test_c_has_member(sizeof(wbl_core_api)), 1);
	EXPECT_EQ(wbl_test_c_has_member(sizeof(std::uint32_t) * 2), 0);
	EXPECT_EQ(wbl_test_c_rgba(), 0x44112233u);
}
