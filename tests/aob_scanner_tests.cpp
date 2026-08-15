#include "core/aob_scanner.h"

#include <gtest/gtest.h>

using warbandlib::core::Find;
using warbandlib::core::ParsePattern;

TEST(AobScanner, FindsExactMatch) {
	const std::uint8_t haystack[] = {0x11, 0x22, 0x33, 0x44, 0x55};
	const auto result = Find(haystack, sizeof(haystack), "22 33 44");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, 1u);
}

TEST(AobScanner, FindsMatchWithWildcards) {
	const std::uint8_t haystack[] = {0x11, 0x22, 0x33, 0x44, 0x55};
	const auto result = Find(haystack, sizeof(haystack), "22 ?? 44");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, 1u);
}

TEST(AobScanner, ReturnsNulloptWhenNotFound) {
	const std::uint8_t haystack[] = {0x11, 0x22, 0x33, 0x44, 0x55};
	const auto result = Find(haystack, sizeof(haystack), "AA BB");
	EXPECT_FALSE(result.has_value());
}

TEST(AobScanner, MatchAtVeryEnd) {
	const std::uint8_t haystack[] = {0x11, 0x22, 0x33, 0x44, 0x55};
	const auto result = Find(haystack, sizeof(haystack), "44 55");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, 3u);
}

TEST(AobScanner, PatternLongerThanHaystackReturnsNullopt) {
	const std::uint8_t haystack[] = {0x11, 0x22};
	const auto result = Find(haystack, sizeof(haystack), "11 22 33 44");
	EXPECT_FALSE(result.has_value());
}

TEST(AobScanner, ParsePatternHandlesSingleQuestionMark) {
	const auto pattern = ParsePattern("11 ? 33");
	ASSERT_EQ(pattern.size(), 3u);
	EXPECT_FALSE(pattern[0].is_wildcard);
	EXPECT_TRUE(pattern[1].is_wildcard);
	EXPECT_FALSE(pattern[2].is_wildcard);
}

TEST(AobScanner, ParsePatternRejectsEmptyInput) {
	EXPECT_THROW(ParsePattern(""), std::invalid_argument);
}

TEST(AobScanner, ParsePatternRejectsDanglingNibble) {
	EXPECT_THROW(ParsePattern("11 2"), std::invalid_argument);
}
