#include <doctest/doctest.h>

#include <godot_cpp/variant/vector3.hpp>

#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdUtils/pipeline.h>

#include <translate/mesh_convert.h>

#include <vector>
#include <limits>
#include <optional>

static std::string fixture_path(const char *name) { return std::string(GODOT_USD_TEST_FIXTURE_DIR) + "/" + name; }

static void check_int32_vectors_equal(const std::vector<int32_t> &actual, const std::vector<int32_t> &expected) {
	REQUIRE(actual.size() == expected.size());
	for (size_t i = 0; i < expected.size(); ++i) {
		CAPTURE(i);
		CHECK(actual[i] == expected[i]);
	}
}

TEST_CASE("mesh_convert: triangulate single tri returns [0, 1, 2]") {
	auto face_counts = PXR_NS::VtArray<int>(1, 3);
	auto tris = godot_usd::translate::triangulate(face_counts);
	check_int32_vectors_equal(tris, std::vector<int32_t>{0, 1, 2});
}

TEST_CASE("mesh_convert: triangulate empty array returns empty") {
	auto face_counts = PXR_NS::VtArray<int>();
	auto tris = godot_usd::translate::triangulate(face_counts);
	CHECK(tris.size() == 0);
}

TEST_CASE("mesh_convert: triangulate degenerate face only returns empty") {
	auto face_counts = PXR_NS::VtArray<int>(1, 2);
	auto tris = godot_usd::translate::triangulate(face_counts);
	CHECK(tris.size() == 0);
}

TEST_CASE("mesh_convert: triangulate quad") {
	auto face_counts = PXR_NS::VtArray<int>(1, 4);
	auto tris = godot_usd::translate::triangulate(face_counts);
	check_int32_vectors_equal(tris, std::vector<int32_t>{0, 1, 2, 0, 2, 3});
}

TEST_CASE("mesh_convert: triangulate quad, 5-gon") {
	auto face_counts = PXR_NS::VtArray<int>(2);
	face_counts[0] = 4;
	face_counts[1] = 5;
	auto tris = godot_usd::translate::triangulate(face_counts);
	check_int32_vectors_equal(tris, std::vector<int32_t>{0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 4, 7, 8});
}

TEST_CASE("mesh_convert: triangulate skips degenerate face") {
	auto face_counts = PXR_NS::VtArray<int>(3);
	face_counts[0] = 4;
	face_counts[1] = 2;
	face_counts[2] = 3;
	auto tris = godot_usd::translate::triangulate(face_counts);
	check_int32_vectors_equal(tris, std::vector<int32_t>{0, 1, 2, 0, 2, 3, 6, 7, 8});
}

TEST_CASE("mesh_convert: validate_topology no error or warnings on valid topology") {
	auto face_counts = PXR_NS::VtArray<int>(1, 3);
	uint32_t warn_flags = 0;
	auto err = godot_usd::translate::validate_topology(face_counts, 3, &warn_flags);
	CHECK(err == godot_usd::translate::MeshConvertError::NONE);
	CHECK(warn_flags == 0);
}

TEST_CASE("mesh_convert: validate_topology warns on degenerate faces") {
	auto face_counts = PXR_NS::VtArray<int>(1, 2);
	uint32_t warn_flags = 0;
	auto err = godot_usd::translate::validate_topology(face_counts, 2, &warn_flags);
	CHECK(err == godot_usd::translate::MeshConvertError::NONE);
	CHECK(warn_flags == godot_usd::translate::MESH_CONVERT_WARN_DEGENERATE_FACES);
}

TEST_CASE("mesh_convert: validate_topology warns on unused face indices") {
	auto face_counts = PXR_NS::VtArray<int>(1, 3);
	uint32_t warn_flags = 0;
	auto err = godot_usd::translate::validate_topology(face_counts, 4, &warn_flags);
	CHECK(err == godot_usd::translate::MeshConvertError::NONE);
	CHECK(warn_flags == godot_usd::translate::MESH_CONVERT_WARN_UNUSED_FACE_INDICES);
}

TEST_CASE("mesh_convert: validate_topology fails on negative face counts") {
	auto face_counts = PXR_NS::VtArray<int>(1, -1);
	uint32_t warn_flags = 0;
	auto err = godot_usd::translate::validate_topology(face_counts, 0, &warn_flags);
	CHECK(err == godot_usd::translate::MeshConvertError::NEGATIVE_FACE_COUNT);
	CHECK(warn_flags == 0);
}

TEST_CASE("mesh_convert: validate_topology fails on corner index out of range") {
	auto face_counts = PXR_NS::VtArray<int>(1, 3);
	uint32_t warn_flags = 0;
	auto err = godot_usd::translate::validate_topology(face_counts, 2, &warn_flags);
	CHECK(err == godot_usd::translate::MeshConvertError::CORNER_INDEX_OUT_OF_RANGE);
	CHECK(warn_flags == 0);
}

TEST_CASE("mesh_convert: validate_topology sets multiple warning flags") {
	auto face_counts = PXR_NS::VtArray<int>{1, 2, 4};
	uint32_t warn_flags = 0;
	auto err = godot_usd::translate::validate_topology(face_counts, 8, &warn_flags);
	CHECK(err == godot_usd::translate::MeshConvertError::NONE);
	CHECK((warn_flags & godot_usd::translate::MESH_CONVERT_WARN_DEGENERATE_FACES) != 0);
	CHECK((warn_flags & godot_usd::translate::MESH_CONVERT_WARN_UNUSED_FACE_INDICES) != 0);
}

TEST_CASE("mesh_convert: validate_topology computes correct corner count") {
	auto face_counts = PXR_NS::VtArray<int>{1, 2, 4};
	size_t corner_count = 0;
	auto err = godot_usd::translate::validate_topology(face_counts, 8, nullptr, &corner_count);
	CHECK(err == godot_usd::translate::MeshConvertError::NONE);
	CHECK(corner_count == 7);
}

TEST_CASE("mesh_convert: validate_topology with no warn_flags pointer succeeds") {
	auto face_counts = PXR_NS::VtArray<int>{0, 2, 4};
	auto err = godot_usd::translate::validate_topology(face_counts, 6);
	CHECK(err == godot_usd::translate::MeshConvertError::NONE);
}

TEST_CASE("mesh_convert: quad_mesh.usda triangulates correctly") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("quad_mesh.usda"));
	REQUIRE(stage != nullptr);
	auto usd_mesh = PXR_NS::UsdGeomMesh(stage->GetPrimAtPath(PXR_NS::SdfPath("/Root/Quad")));
	REQUIRE(static_cast<bool>(usd_mesh));
	auto face_counts = PXR_NS::VtArray<int>();
	CHECK(usd_mesh.GetFaceVertexCountsAttr().Get(&face_counts));
	auto tris = godot_usd::translate::triangulate(face_counts);
	check_int32_vectors_equal(tris, std::vector<int32_t>{0, 1, 2, 0, 2, 3});
}

TEST_CASE("mesh_convert: quad_mesh.usda converts correctly") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("quad_mesh.usda"));
	REQUIRE(stage != nullptr);
	auto mesh_data = godot_usd::translate::to_mesh_data(stage->GetPrimAtPath(PXR_NS::SdfPath("/Root/Quad")));
	CHECK(mesh_data.is_valid());
	CHECK(mesh_data.warn_flags == 0);
	check_int32_vectors_equal(mesh_data.triangles, std::vector<int32_t>{0, 1, 2, 0, 2, 3});
	CHECK(mesh_data.positions.size() == 4);
	CHECK(mesh_data.positions[0] == godot::Vector3(-1, 0, -1));
	CHECK(mesh_data.positions[1] == godot::Vector3(-1, 0, 1));
	CHECK(mesh_data.positions[2] == godot::Vector3(1, 0, 1));
	CHECK(mesh_data.positions[3] == godot::Vector3(1, 0, -1));
}

TEST_CASE("mesh_convert: truncated_face_indices.usda fails to convert with degenerate faces warning and corner index "
		  "out of range error") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("truncated_face_indices.usda"));
	REQUIRE(stage != nullptr);
	auto mesh_data =
			godot_usd::translate::to_mesh_data(stage->GetPrimAtPath(PXR_NS::SdfPath("/Root/TruncatedIndices")));
	CHECK(mesh_data.warn_flags == godot_usd::translate::MESH_CONVERT_WARN_DEGENERATE_FACES);
	CHECK(!mesh_data.is_valid());
	CHECK(mesh_data.error_reason == godot_usd::translate::MeshConvertError::CORNER_INDEX_OUT_OF_RANGE);
}

TEST_CASE("mesh_convert: map_corners_to_points non-identity mapping with differing array sizes maps correctly") {
	auto tri_indices = std::vector<int32_t>{0, 1, 2, 0, 2, 3};
	auto face_vertex_indices = PXR_NS::VtArray<int>{10, 11, 12, 13};
	CHECK(godot_usd::translate::map_corners_to_points(tri_indices, face_vertex_indices, 20) ==
			godot_usd::translate::MeshConvertError::NONE);
	check_int32_vectors_equal(tri_indices, std::vector<int32_t>{10, 11, 12, 10, 12, 13});
}

TEST_CASE("mesh_convert: map_corners_to_points empty triangles with non-empty indices succeeds no-op") {
	auto tri_indices = std::vector<int32_t>();
	auto face_vertex_indices = PXR_NS::VtArray<int>(6);
	CHECK(godot_usd::translate::map_corners_to_points(tri_indices, face_vertex_indices,
				  std::numeric_limits<int>::max()) == godot_usd::translate::MeshConvertError::NONE);
}

TEST_CASE("mesh_convert: map_corners_to_points corner offset past the end fails") {
	auto tri_indices = std::vector<int32_t>{0, 1, 2};
	auto face_vertex_indices = PXR_NS::VtArray<int>(2);
	CHECK(godot_usd::translate::map_corners_to_points(
				  tri_indices, face_vertex_indices, std::numeric_limits<int>::max()) ==
			godot_usd::translate::MeshConvertError::CORNER_INDEX_OUT_OF_RANGE);
}

TEST_CASE("mesh_convert: map_corners_to_points point index out of range fails") {
	auto tri_indices = std::vector<int32_t>{0};
	auto face_vertex_indices = PXR_NS::VtArray<int>(1, 9);
	CHECK(godot_usd::translate::map_corners_to_points(tri_indices, face_vertex_indices, 4) ==
			godot_usd::translate::MeshConvertError::POINT_INDEX_OUT_OF_RANGE);
}

TEST_CASE("mesh_convert: map_corners_to_points negative point index fails") {
	auto tri_indices = std::vector<int32_t>{0};
	auto face_vertex_indices = PXR_NS::VtArray<int>(1, -1);
	CHECK(godot_usd::translate::map_corners_to_points(tri_indices, face_vertex_indices,
				  std::numeric_limits<int>::max()) == godot_usd::translate::MeshConvertError::POINT_INDEX_OUT_OF_RANGE);
}

TEST_CASE("mesh_convert: shared_edge_quads.usda converts correctly") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("shared_edge_quads.usda"));
	REQUIRE(stage != nullptr);
	auto mesh_data = godot_usd::translate::to_mesh_data(stage->GetPrimAtPath(PXR_NS::SdfPath("/Root/SharedEdgeQuads")));
	CHECK(mesh_data.is_valid());
	CHECK(mesh_data.warn_flags == 0);
	check_int32_vectors_equal(mesh_data.triangles, std::vector<int32_t>{0, 1, 2, 0, 2, 3, 3, 2, 4, 3, 4, 5});
	CHECK(mesh_data.positions.size() == 6);
	CHECK(mesh_data.positions[0] == godot::Vector3(-2, 0, -1));
	CHECK(mesh_data.positions[1] == godot::Vector3(-2, 0, 1));
	CHECK(mesh_data.positions[2] == godot::Vector3(0, 0, 1));
	CHECK(mesh_data.positions[3] == godot::Vector3(0, 0, -1));
	CHECK(mesh_data.positions[4] == godot::Vector3(2, 0, 1));
	CHECK(mesh_data.positions[5] == godot::Vector3(2, 0, -1));
}

TEST_CASE("mesh_convert: get_interpolation_mode returns correct values for valid tokens") {
	auto constant_mode = godot_usd::translate::get_interpolation_mode(PXR_NS::UsdGeomTokens->constant);
	CHECK(constant_mode.has_value());
	CHECK(constant_mode.value() == godot_usd::translate::InterpolationMode::CONSTANT);
	auto uniform_mode = godot_usd::translate::get_interpolation_mode(PXR_NS::UsdGeomTokens->uniform);
	CHECK(uniform_mode.has_value());
	CHECK(uniform_mode.value() == godot_usd::translate::InterpolationMode::UNIFORM);
	auto varying_mode = godot_usd::translate::get_interpolation_mode(PXR_NS::UsdGeomTokens->varying);
	CHECK(varying_mode.has_value());
	CHECK(varying_mode.value() == godot_usd::translate::InterpolationMode::VARYING);
	auto vertex_mode = godot_usd::translate::get_interpolation_mode(PXR_NS::UsdGeomTokens->vertex);
	CHECK(vertex_mode.has_value());
	CHECK(vertex_mode.value() == godot_usd::translate::InterpolationMode::VERTEX);
	auto face_varying_mode = godot_usd::translate::get_interpolation_mode(PXR_NS::UsdGeomTokens->faceVarying);
	CHECK(face_varying_mode.has_value());
	CHECK(face_varying_mode.value() == godot_usd::translate::InterpolationMode::FACE_VARYING);
}

TEST_CASE("mesh_convert: get_interpolation_mode returns nullopt for invalid token") {
	auto invalid_mode = godot_usd::translate::get_interpolation_mode(PXR_NS::TfToken("invalid_token"));
	CHECK(!invalid_mode.has_value());
}

TEST_CASE("mesh_convert: expected_value_count returns correct values for valid interpolation modes") {
	CHECK(godot_usd::translate::expected_value_count(godot_usd::translate::InterpolationMode::CONSTANT, 10, 20, 30) ==
			1);
	CHECK(godot_usd::translate::expected_value_count(godot_usd::translate::InterpolationMode::UNIFORM, 10, 20, 30) ==
			20);
	CHECK(godot_usd::translate::expected_value_count(godot_usd::translate::InterpolationMode::VARYING, 10, 20, 30) ==
			10);
	CHECK(godot_usd::translate::expected_value_count(godot_usd::translate::InterpolationMode::VERTEX, 10, 20, 30) ==
			10);
	CHECK(godot_usd::translate::expected_value_count(
				  godot_usd::translate::InterpolationMode::FACE_VARYING, 10, 20, 30) == 30);
}

TEST_CASE("mesh_convert: expected_value_count returns correct values for empty mesh") {
	CHECK(godot_usd::translate::expected_value_count(godot_usd::translate::InterpolationMode::CONSTANT, 0, 0, 0) == 1);
	CHECK(godot_usd::translate::expected_value_count(godot_usd::translate::InterpolationMode::UNIFORM, 0, 0, 0) == 0);
	CHECK(godot_usd::translate::expected_value_count(godot_usd::translate::InterpolationMode::VARYING, 0, 0, 0) == 0);
	CHECK(godot_usd::translate::expected_value_count(godot_usd::translate::InterpolationMode::VERTEX, 0, 0, 0) == 0);
	CHECK(godot_usd::translate::expected_value_count(godot_usd::translate::InterpolationMode::FACE_VARYING, 0, 0, 0) ==
			0);
}

TEST_CASE("mesh_convert: unauthored_interpolation.usda returns expected default interpolation modes for normals and uv "
		  "attributes") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("unauthored_interpolation.usda"));
	REQUIRE(stage != nullptr);
	auto prim = stage->GetPrimAtPath(PXR_NS::SdfPath("/Root/UnauthoredInterpolation"));
	REQUIRE(prim.IsValid());
	auto primvars_api = PXR_NS::UsdGeomPrimvarsAPI(prim);
	REQUIRE(primvars_api.HasPrimvar(PXR_NS::UsdUtilsGetPrimaryUVSetName()));
	CHECK(primvars_api.GetPrimvar(PXR_NS::UsdUtilsGetPrimaryUVSetName()).GetInterpolation() ==
			PXR_NS::UsdGeomTokens->constant);
	auto usd_mesh = PXR_NS::UsdGeomMesh(prim);
	REQUIRE(static_cast<bool>(usd_mesh));
	CHECK(usd_mesh.GetNormalsInterpolation() == PXR_NS::UsdGeomTokens->vertex);
}

TEST_CASE("mesh_convert: corner_to_face gives expected output for {4, 0, 3}") {
	auto face_counts = PXR_NS::VtArray<int>{4, 0, 3};
	auto corner_to_face_indices = godot_usd::translate::corner_to_face(face_counts);
	check_int32_vectors_equal(corner_to_face_indices, std::vector<int32_t>{0, 0, 0, 0, 2, 2, 2});
}

TEST_CASE("mesh_convert: corner_to_face gives expected output for {4, 2, 3}") {
	auto face_counts = PXR_NS::VtArray<int>{4, 2, 3};
	auto corner_to_face_indices = godot_usd::translate::corner_to_face(face_counts);
	check_int32_vectors_equal(corner_to_face_indices, std::vector<int32_t>{0, 0, 0, 0, 1, 1, 2, 2, 2});
}

TEST_CASE("mesh_convert: corner_to_face returns empty for {}") {
	auto face_counts = PXR_NS::VtArray<int>();
	auto corner_to_face_indices = godot_usd::translate::corner_to_face(face_counts);
	CHECK(corner_to_face_indices.size() == 0);
}

TEST_CASE("mesh_convert: sample_value_indices gives expected output for CONSTANT interpolation") {
	auto tri_indices = std::vector<int32_t>{2, 0, 1};
	auto face_vertex_indices = PXR_NS::VtArray<int>(3);
	auto value_indices = godot_usd::translate::sample_value_indices(
			godot_usd::translate::InterpolationMode::CONSTANT, tri_indices, std::nullopt, face_vertex_indices);
	check_int32_vectors_equal(value_indices, std::vector<int32_t>{0, 0, 0});
}

TEST_CASE("mesh_convert: sample_value_indices gives expected output for UNIFORM interpolation") {
	auto tri_indices = std::vector<int32_t>{2, 0, 1};
	auto corner_to_face = std::vector<int32_t>{3, 4, 5};
	auto face_vertex_indices = PXR_NS::VtArray<int>(3);
	auto value_indices = godot_usd::translate::sample_value_indices(
			godot_usd::translate::InterpolationMode::UNIFORM, tri_indices, corner_to_face, face_vertex_indices);
	check_int32_vectors_equal(value_indices, std::vector<int32_t>{5, 3, 4});
}

TEST_CASE("mesh_convert: sample_value_indices throws bad_optional_access for UNIFORM interpolation with no "
		  "corner_to_face parameter supplied") {
	auto tri_indices = std::vector<int32_t>{2, 0, 1};
	auto face_vertex_indices = PXR_NS::VtArray<int>(3);
	CHECK_THROWS_AS(godot_usd::translate::sample_value_indices(godot_usd::translate::InterpolationMode::UNIFORM,
							tri_indices, std::nullopt, face_vertex_indices),
			std::bad_optional_access);
}

TEST_CASE("mesh_convert: sample_value_indices gives expected output for VARYING interpolation") {
	auto tri_indices = std::vector<int32_t>{2, 0, 1};
	auto face_vertex_indices = PXR_NS::VtArray<int>{6, 7, 8};
	auto value_indices = godot_usd::translate::sample_value_indices(
			godot_usd::translate::InterpolationMode::VARYING, tri_indices, std::nullopt, face_vertex_indices);
	check_int32_vectors_equal(value_indices, std::vector<int32_t>{8, 6, 7});
}

TEST_CASE("mesh_convert: sample_value_indices gives expected output for VERTEX interpolation") {
	auto tri_indices = std::vector<int32_t>{2, 0, 1};
	auto face_vertex_indices = PXR_NS::VtArray<int>{6, 7, 8};
	auto value_indices = godot_usd::translate::sample_value_indices(
			godot_usd::translate::InterpolationMode::VERTEX, tri_indices, std::nullopt, face_vertex_indices);
	check_int32_vectors_equal(value_indices, std::vector<int32_t>{8, 6, 7});
}

TEST_CASE("mesh_convert: sample_value_indices gives expected output for FACE_VARYING interpolation") {
	auto tri_indices = std::vector<int32_t>{2, 0, 1};
	auto face_vertex_indices = PXR_NS::VtArray<int>{6, 7, 8};
	auto value_indices = godot_usd::translate::sample_value_indices(
			godot_usd::translate::InterpolationMode::FACE_VARYING, tri_indices, std::nullopt, face_vertex_indices);
	check_int32_vectors_equal(value_indices, std::vector<int32_t>{2, 0, 1});
}
