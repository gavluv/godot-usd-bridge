#include <doctest/doctest.h>

#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/imageable.h>

#include <translate/imageable.h>

#include <map>
#include <optional>

static std::string fixture_path(const char *name) { return std::string(GODOT_USD_TEST_FIXTURE_DIR) + "/" + name; }

struct ExpectedImageableData {
	std::optional<bool> visible;
	std::optional<PXR_NS::TfToken> purpose;
	uint32_t warn_flags = 0;
};

static void check_purpose_and_visibility_inner(const PXR_NS::UsdPrim &p_prim,
		const PXR_NS::UsdGeomImageable::PurposeInfo &p_parent_purpose_info,
		const std::map<PXR_NS::SdfPath, ExpectedImageableData> &p_expected_imageable_data,
		PXR_NS::SdfPathSet *r_visited_set,
		const PXR_NS::TfToken::Set &p_hide_purposes = godot_usd::translate::default_hide_purposes) {
	REQUIRE(p_prim.IsValid());
	auto prim_path = p_prim.GetPath();
	CAPTURE(prim_path.GetString());
	auto imageable_data = godot_usd::translate::to_imageable_data(p_prim, p_parent_purpose_info, p_hide_purposes);
	auto it = p_expected_imageable_data.find(prim_path);
	if (it != p_expected_imageable_data.end()) {
		if (it->second.visible.has_value()) {
			CHECK(imageable_data.visible == it->second.visible.value());
		}
		if (it->second.purpose.has_value()) {
			CHECK(imageable_data.purpose_info.purpose == it->second.purpose.value());
		}
		CHECK(imageable_data.warn_flags == it->second.warn_flags);
		r_visited_set->insert(prim_path);
	} else {
		CHECK(imageable_data.warn_flags == 0);
	}
	for (PXR_NS::UsdPrim child_prim : p_prim.GetChildren()) {
		check_purpose_and_visibility_inner(
				child_prim, imageable_data.purpose_info, p_expected_imageable_data, r_visited_set, p_hide_purposes);
	}
}

static void check_purpose_and_visibility(const PXR_NS::UsdStageWeakPtr &p_stage,
		const std::map<PXR_NS::SdfPath, ExpectedImageableData> &p_expected_imageable_data,
		const PXR_NS::TfToken::Set &p_hide_purposes = godot_usd::translate::default_hide_purposes) {
	REQUIRE(p_stage != nullptr);
	auto purpose_info = PXR_NS::UsdGeomImageable::PurposeInfo();
	auto visited_set = PXR_NS::SdfPathSet();
	check_purpose_and_visibility_inner(
			p_stage->GetPseudoRoot(), purpose_info, p_expected_imageable_data, &visited_set, p_hide_purposes);
	if (visited_set.size() != p_expected_imageable_data.size()) {
		for (const auto &kv : p_expected_imageable_data) {
			if (visited_set.find(kv.first) == visited_set.end()) {
				CAPTURE(kv.first.GetString());
				FAIL_CHECK("Expected prim was not visited during traversal.");
			}
		}
	}
}

TEST_CASE("imageable: to_imageable_data computes correct purpose and visibility for invisible_subtree.usda") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("invisible_subtree.usda"));
	auto expected_map = std::map<PXR_NS::SdfPath, ExpectedImageableData>();
	expected_map[PXR_NS::SdfPath("/Root/Hidden")] = ExpectedImageableData{false, PXR_NS::TfToken("default")};
	expected_map[PXR_NS::SdfPath("/Root/Hidden/ChildInherited")] =
			ExpectedImageableData{true, PXR_NS::TfToken("default")};
	expected_map[PXR_NS::SdfPath("/Root/Sibling")] = ExpectedImageableData{true, PXR_NS::TfToken("default")};
	check_purpose_and_visibility(stage, expected_map);
}

TEST_CASE("imageable: to_imageable_data computes correct purpose and visibility for guide_purpose.usda") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("guide_purpose.usda"));
	auto expected_map = std::map<PXR_NS::SdfPath, ExpectedImageableData>();
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup")] = ExpectedImageableData{true, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup/SilentMesh")] =
			ExpectedImageableData{false, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup/DefaultMesh")] =
			ExpectedImageableData{true, PXR_NS::TfToken("default")};
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup/GuideCube")] =
			ExpectedImageableData{false, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup/Untyped")] = ExpectedImageableData{true, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup/Untyped/Grandchild")] =
			ExpectedImageableData{false, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideSkelRoot")] = ExpectedImageableData{true, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideSkelRoot/Body")] = ExpectedImageableData{true, PXR_NS::TfToken("default")};
	expected_map[PXR_NS::SdfPath("/Root/RenderMesh")] = ExpectedImageableData{true, PXR_NS::TfToken("render")};
	expected_map[PXR_NS::SdfPath("/Root/ProxyMesh")] = ExpectedImageableData{true, PXR_NS::TfToken("proxy")};
	check_purpose_and_visibility(stage, expected_map);
}

TEST_CASE("imageable: to_imageable_data computes correct purpose and visibility for guide_purpose.usda, with "
		  "non-default hide_purposes") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("guide_purpose.usda"));
	auto expected_map = std::map<PXR_NS::SdfPath, ExpectedImageableData>();
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup")] = ExpectedImageableData{true, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup/SilentMesh")] =
			ExpectedImageableData{true, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup/DefaultMesh")] =
			ExpectedImageableData{true, PXR_NS::TfToken("default")};
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup/GuideCube")] = ExpectedImageableData{true, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup/Untyped")] = ExpectedImageableData{true, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup/Untyped/Grandchild")] =
			ExpectedImageableData{true, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideSkelRoot")] = ExpectedImageableData{true, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideSkelRoot/Body")] = ExpectedImageableData{true, PXR_NS::TfToken("default")};
	expected_map[PXR_NS::SdfPath("/Root/RenderMesh")] = ExpectedImageableData{false, PXR_NS::TfToken("render")};
	expected_map[PXR_NS::SdfPath("/Root/ProxyMesh")] = ExpectedImageableData{true, PXR_NS::TfToken("proxy")};
	check_purpose_and_visibility(stage, expected_map, PXR_NS::TfToken::Set{PXR_NS::TfToken("render")});
}

TEST_CASE("imageable: to_imageable_data computes correct purpose and visibility for guide_invisible.usda") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("guide_invisible.usda"));
	auto expected_map = std::map<PXR_NS::SdfPath, ExpectedImageableData>();
	expected_map[PXR_NS::SdfPath("/Root/GuideInvisible")] = ExpectedImageableData{false, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideInvisible/DefaultMesh")] =
			ExpectedImageableData{true, PXR_NS::TfToken("default")};
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup")] = ExpectedImageableData{true, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup/InvisibleXform")] =
			ExpectedImageableData{false, PXR_NS::TfToken("guide")};
	expected_map[PXR_NS::SdfPath("/Root/GuideGroup/InvisibleXform/DefaultMesh")] =
			ExpectedImageableData{true, PXR_NS::TfToken("default")};
	check_purpose_and_visibility(stage, expected_map);
}

TEST_CASE("imageable: to_imageable_data computes correct purpose and visibility and gives expected warnings for "
		  "invalid_imageable_tokens.usda") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("invalid_imageable_tokens.usda"));
	auto expected_map = std::map<PXR_NS::SdfPath, ExpectedImageableData>();
	expected_map[PXR_NS::SdfPath("/Root/WrongCaseVisibility")] = ExpectedImageableData{
			true, PXR_NS::TfToken("default"), godot_usd::translate::IMAGEABLE_CONVERT_WARN_VISIBILITY_INVALID};
	expected_map[PXR_NS::SdfPath("/Root/VisibleToken")] = ExpectedImageableData{
			true, PXR_NS::TfToken("default"), godot_usd::translate::IMAGEABLE_CONVERT_WARN_VISIBILITY_INVALID};
	expected_map[PXR_NS::SdfPath("/Root/BogusPurpose")] = ExpectedImageableData{
			true, PXR_NS::TfToken("Guide"), godot_usd::translate::IMAGEABLE_CONVERT_WARN_PURPOSE_INVALID};
	expected_map[PXR_NS::SdfPath("/Root/EmptyPurpose")] = ExpectedImageableData{
			true, PXR_NS::TfToken(""), godot_usd::translate::IMAGEABLE_CONVERT_WARN_PURPOSE_INVALID};
	expected_map[PXR_NS::SdfPath("/Root/BogusPurpose/InheritsBogus")] =
			ExpectedImageableData{true, PXR_NS::TfToken("Guide")};
	expected_map[PXR_NS::SdfPath("/Root/BlockedVisibility")] = ExpectedImageableData{true, PXR_NS::TfToken("default")};
	expected_map[PXR_NS::SdfPath("/Root/DeclaredVisibility")] = ExpectedImageableData{true, PXR_NS::TfToken("default")};
	expected_map[PXR_NS::SdfPath("/Root/TimeSampledVisibility")] =
			ExpectedImageableData{true, PXR_NS::TfToken("default")};
	check_purpose_and_visibility(stage, expected_map);
}

TEST_CASE("imageable: to_imageable_data computes correct purpose and visibility for untyped_imageable_opinions.usda") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("untyped_imageable_opinions.usda"));
	auto expected_map = std::map<PXR_NS::SdfPath, ExpectedImageableData>();
	expected_map[PXR_NS::SdfPath("/Root/Untyped")] = ExpectedImageableData{true, PXR_NS::TfToken("default")};
	expected_map[PXR_NS::SdfPath("/Root/Untyped/Child")] = ExpectedImageableData{true, PXR_NS::TfToken("default")};
	check_purpose_and_visibility(stage, expected_map);
}
