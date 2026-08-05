#include <doctest/doctest.h>

#include <godot_cpp/variant/vector3.hpp>

#include <pxr/base/vt/array.h>

#include <translate/mesh_convert.h>

#include <vector>
#include <limits>

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
	auto usd_mesh = PXR_NS::UsdGeomMesh(stage->GetPrimAtPath(PXR_NS::SdfPath("/Root/Quad")));
	REQUIRE(static_cast<bool>(usd_mesh));
	auto mesh_data = godot_usd::translate::to_mesh_data(usd_mesh);
	CHECK(mesh_data.is_valid);
	check_int32_vectors_equal(mesh_data.triangles, std::vector<int32_t>{0, 1, 2, 0, 2, 3});
	CHECK(mesh_data.points.size() == 4);
	CHECK(mesh_data.points[0] == godot::Vector3(-1, 0, -1));
	CHECK(mesh_data.points[1] == godot::Vector3(-1, 0, 1));
	CHECK(mesh_data.points[2] == godot::Vector3(1, 0, 1));
	CHECK(mesh_data.points[3] == godot::Vector3(1, 0, -1));
}

TEST_CASE("mesh_convert: map_corners_to_points non-identity mapping with differing array sizes maps correctly") {
	auto tri_indices = std::vector<int32_t>{0, 1, 2, 0, 2, 3};
	auto face_vertex_indices = PXR_NS::VtArray<int>{10, 11, 12, 13};
	CHECK(godot_usd::translate::map_corners_to_points(tri_indices, face_vertex_indices, 20));
	check_int32_vectors_equal(tri_indices, std::vector<int32_t>{10, 11, 12, 10, 12, 13});
}

TEST_CASE("mesh_convert: map_corners_to_points empty triangles with non-empty indices succeeds no-op") {
	auto tri_indices = std::vector<int32_t>();
	auto face_vertex_indices = PXR_NS::VtArray<int>(6);
	CHECK(godot_usd::translate::map_corners_to_points(
			tri_indices, face_vertex_indices, std::numeric_limits<int>::max()));
}

TEST_CASE("mesh_convert: map_corners_to_points corner offset past the end fails") {
	auto tri_indices = std::vector<int32_t>{0, 1, 2};
	auto face_vertex_indices = PXR_NS::VtArray<int>(2);
	CHECK(!godot_usd::translate::map_corners_to_points(
			tri_indices, face_vertex_indices, std::numeric_limits<int>::max()));
}

TEST_CASE("mesh_convert: map_corners_to_points point index out of range fails") {
	auto tri_indices = std::vector<int32_t>{0};
	auto face_vertex_indices = PXR_NS::VtArray<int>(1, 9);
	CHECK(!godot_usd::translate::map_corners_to_points(tri_indices, face_vertex_indices, 4));
}

TEST_CASE("mesh_convert: map_corners_to_points negative point index fails") {
	auto tri_indices = std::vector<int32_t>{0};
	auto face_vertex_indices = PXR_NS::VtArray<int>(1, -1);
	CHECK(!godot_usd::translate::map_corners_to_points(
			tri_indices, face_vertex_indices, std::numeric_limits<int>::max()));
}

TEST_CASE("mesh_convert: shared_edge_quads.usda converts correctly") {
	PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(fixture_path("shared_edge_quads.usda"));
	REQUIRE(stage != nullptr);
	auto usd_mesh = PXR_NS::UsdGeomMesh(stage->GetPrimAtPath(PXR_NS::SdfPath("/Root/SharedEdgeQuads")));
	REQUIRE(static_cast<bool>(usd_mesh));
	auto mesh_data = godot_usd::translate::to_mesh_data(usd_mesh);
	CHECK(mesh_data.is_valid);
	check_int32_vectors_equal(mesh_data.triangles, std::vector<int32_t>{0, 1, 2, 0, 2, 3, 3, 2, 4, 3, 4, 5});
	CHECK(mesh_data.points.size() == 6);
	CHECK(mesh_data.points[0] == godot::Vector3(-2, 0, -1));
	CHECK(mesh_data.points[1] == godot::Vector3(-2, 0, 1));
	CHECK(mesh_data.points[2] == godot::Vector3(0, 0, 1));
	CHECK(mesh_data.points[3] == godot::Vector3(0, 0, -1));
	CHECK(mesh_data.points[4] == godot::Vector3(2, 0, 1));
	CHECK(mesh_data.points[5] == godot::Vector3(2, 0, -1));
}
