#include <doctest/doctest.h>

#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/core/math_defs.hpp>

#include <pxr/base/tf/token.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usd/stage.h>

#include <translate/conventions.h>

static std::string fixture_path(const char *name) {
	return std::string(GODOT_USD_TEST_FIXTURE_DIR) + "/" + name;
}

TEST_CASE("conventions: Y-up, 1 meter per unit returns identity root transform") {
	auto root_transform = godot_usd::translate::root_transform(1.0, PXR_NS::UsdGeomTokens->y);
	auto expected = godot::Transform3D();
	CHECK(root_transform.is_equal_approx(expected));
}

TEST_CASE("conventions: Z-up, 1 meter per unit returns a transform that rotates -90 degrees on the X axis") {
	auto root_transform = godot_usd::translate::root_transform(1.0, PXR_NS::UsdGeomTokens->z);
	CHECK(root_transform.basis.xform(godot::Vector3(0, 0, 1)).is_equal_approx(godot::Vector3(0, 1, 0)));
	CHECK(root_transform.basis.xform(godot::Vector3(0, 1, 0)).is_equal_approx(godot::Vector3(0, 0, -1)));
}

TEST_CASE("conventions: authored Y-up, 1 cm per unit returns correct stage conventions"){
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::CreateInMemory();
	PXR_NS::UsdGeomSetStageMetersPerUnit(stage, 0.01);
	PXR_NS::UsdGeomSetStageUpAxis(stage, PXR_NS::UsdGeomTokens->y);
	auto conventions = godot_usd::translate::read_stage_conventions(stage);
	CHECK(conventions.meters_per_unit == 0.01);
	CHECK(conventions.up_axis == PXR_NS::UsdGeomTokens->y);
	CHECK(!conventions.up_axis_unauthored);
	CHECK(!conventions.up_axis_malformed);
}

TEST_CASE("conventions: unauthored up axis and meters per unit returns correct stage conventions"){
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::CreateInMemory();
	auto conventions = godot_usd::translate::read_stage_conventions(stage);
	CHECK(conventions.meters_per_unit == 0.01);
	CHECK(conventions.up_axis == PXR_NS::UsdGeomGetFallbackUpAxis());
	CHECK(conventions.up_axis_unauthored);
	CHECK(!conventions.up_axis_malformed);
}

TEST_CASE("conventions: malformed up axis returns correct stage conventions"){
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::CreateInMemory();
	// UsdGeomSetStageUpAxis() rejects values that are not "y" or "z",
	// so we have to set the malformed up axis directly on the stage metadata
	stage->SetMetadata(PXR_NS::UsdGeomTokens->upAxis, PXR_NS::TfToken("a"));
	auto conventions = godot_usd::translate::read_stage_conventions(stage);
	CHECK(conventions.up_axis == PXR_NS::UsdGeomGetFallbackUpAxis());
	CHECK(!conventions.up_axis_unauthored);
	CHECK(conventions.up_axis_malformed);
}

TEST_CASE("conventions: y_up_cm.usda returns correct root transform") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("y_up_cm.usda"));
	REQUIRE(stage != nullptr);
	auto root_transform = godot_usd::translate::root_transform(stage);
	auto expected = godot::Transform3D(godot::Basis::from_scale(godot::Vector3(0.01, 0.01, 0.01)));
	CHECK(root_transform.is_equal_approx(expected));
}

TEST_CASE("conventions: z_up_m.usda returns correct root transform") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("z_up_m.usda"));
	REQUIRE(stage != nullptr);
	auto root_transform = godot_usd::translate::root_transform(stage);
	CHECK(root_transform.basis.xform(godot::Vector3(0, 0, 1)).is_equal_approx(godot::Vector3(0, 1, 0)));
	CHECK(root_transform.basis.xform(godot::Vector3(0, 1, 0)).is_equal_approx(godot::Vector3(0, 0, -1)));
}

TEST_CASE("conventions: unauthored_defaults.usda returns correct root transform") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("unauthored_defaults.usda"));
	REQUIRE(stage != nullptr);
	auto root_transform = godot_usd::translate::root_transform(stage);
	auto expected = godot::Transform3D(godot::Basis::from_scale(godot::Vector3(0.01, 0.01, 0.01)));
	if (PXR_NS::UsdGeomGetFallbackUpAxis() == PXR_NS::UsdGeomTokens->z){
		expected = expected.rotated(godot::Vector3(1, 0, 0), -Math_PI / 2.0);
	}
	CHECK(root_transform.is_equal_approx(expected));
}