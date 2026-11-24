#include <gtest/gtest.h>

#include "runtime/host/plugin_graph.h"

using namespace warbandlib::host;

namespace {

PluginNode Node(const char* id, const char* version = "1.0.0", std::vector<PluginDependency> deps = {}) {
	return PluginNode{id, *ParseVersion(version), std::move(deps)};
}

PluginDependency Dep(const char* id, const char* min = nullptr, bool optional = false) {
	PluginDependency d;
	d.id = id;
	if (min != nullptr) d.min_version = ParseVersion(min);
	d.optional = optional;
	return d;
}

bool Rejected(const LoadPlan& plan, std::size_t index) {
	for (const auto& r : plan.rejected)
		if (r.index == index) return true;
	return false;
}

} // namespace

TEST(PluginGraph, ValidIds) {
	EXPECT_TRUE(IsValidPluginId("erano.coop"));
	EXPECT_TRUE(IsValidPluginId("a-b_c.9"));
	EXPECT_FALSE(IsValidPluginId(""));
	EXPECT_FALSE(IsValidPluginId("Upper"));
	EXPECT_FALSE(IsValidPluginId("has space"));
	EXPECT_FALSE(IsValidPluginId(std::string(65, 'a')));
}

TEST(PluginGraph, DependenciesLoadFirstAndTiesKeepInputOrder) {
	const auto plan = PlanLoadOrder({Node("c", "1.0.0", {Dep("b")}), Node("b", "1.0.0", {Dep("a")}), Node("a"), Node("z")});
	EXPECT_TRUE(plan.rejected.empty());
	EXPECT_EQ(plan.order, (std::vector<std::size_t>{2, 1, 0, 3}));
}

TEST(PluginGraph, MissingDependencyRejectsTransitively) {
	const auto plan = PlanLoadOrder({Node("a", "1.0.0", {Dep("missing")}), Node("b", "1.0.0", {Dep("a")}), Node("c")});
	EXPECT_TRUE(Rejected(plan, 0));
	EXPECT_TRUE(Rejected(plan, 1));
	EXPECT_EQ(plan.order, (std::vector<std::size_t>{2}));
}

TEST(PluginGraph, OptionalDependencyOrdersWhenPresentAndIsIgnoredWhenAbsent) {
	const auto plan = PlanLoadOrder({Node("a", "1.0.0", {Dep("b", nullptr, true), Dep("gone", nullptr, true)}), Node("b")});
	EXPECT_TRUE(plan.rejected.empty());
	EXPECT_EQ(plan.order, (std::vector<std::size_t>{1, 0}));
}

TEST(PluginGraph, MinimumVersion) {
	const auto plan = PlanLoadOrder({Node("lib", "1.2.0"), Node("new", "1.0.0", {Dep("lib", "1.3.0")}),
	                                 Node("ok", "1.0.0", {Dep("lib", "1.2.0")})});
	EXPECT_TRUE(Rejected(plan, 1));
	EXPECT_EQ(plan.order, (std::vector<std::size_t>{0, 2}));
}

TEST(PluginGraph, DuplicatesAndCycles) {
	const auto plan = PlanLoadOrder({Node("x"), Node("x"), Node("p", "1.0.0", {Dep("q")}), Node("q", "1.0.0", {Dep("p")})});
	EXPECT_FALSE(Rejected(plan, 0));
	EXPECT_TRUE(Rejected(plan, 1));
	EXPECT_TRUE(Rejected(plan, 2));
	EXPECT_TRUE(Rejected(plan, 3));
	EXPECT_EQ(plan.order, (std::vector<std::size_t>{0}));
}
