#include "mesh_convert.h"

#include <godot_cpp/variant/vector3.hpp>

#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/mesh.h>

#include <vector>
#include <utility>

namespace godot_usd::translate {

bool map_corners_to_points(
		std::vector<int32_t> &r_tri_indices, const PXR_NS::VtArray<int> &p_face_vertex_indices, size_t p_point_count) {
	const int64_t index_count = static_cast<int64_t>(p_face_vertex_indices.size());
	const int64_t point_count = static_cast<int64_t>(p_point_count);

	// mutates r_tri_indices in-place; r_tri_indices holds corner offsets entering, point indices leaving the loop

	for (size_t i = 0; i < r_tri_indices.size(); ++i) {
		if (r_tri_indices[i] >= index_count) {
			return false;
		}
		const int point_index = p_face_vertex_indices[r_tri_indices[i]];
		if (point_index < 0 || point_index >= point_count) {
			return false;
		}
		r_tri_indices[i] = point_index;
	}

	return true;
}

MeshData to_mesh_data(const PXR_NS::VtArray<int> &p_face_vertex_counts,
		const PXR_NS::VtArray<int> &p_face_vertex_indices, const PXR_NS::VtArray<PXR_NS::GfVec3f> &p_points) {
	auto mesh_data = MeshData();

	auto triangles = triangulate(p_face_vertex_counts);

	size_t point_count = p_points.size();

	if (!map_corners_to_points(triangles, p_face_vertex_indices, point_count)) {
		// silently return mesh_data with is_valid = false when encountering an error;
		// can't use the Godot-bridged ERR_PRINT here without crashing the test exe
		return mesh_data;
	}

	auto points = std::vector<godot::Vector3>(point_count);

	for (size_t i = 0; i < p_points.size(); i++) {
		points[i] = godot::Vector3(p_points[i][0], p_points[i][1], p_points[i][2]);
	}

	mesh_data.points = std::move(points);
	mesh_data.triangles = std::move(triangles);
	mesh_data.is_valid = true;

	return mesh_data;
}

MeshData to_mesh_data(const PXR_NS::UsdGeomMesh &p_mesh) {
	if (!static_cast<bool>(p_mesh)) {
		return MeshData();
	}

	auto face_vertex_counts = PXR_NS::VtArray<int>();
	if (!p_mesh.GetFaceVertexCountsAttr().Get(&face_vertex_counts)) {
		return MeshData();
	}

	auto face_vertex_indices = PXR_NS::VtArray<int>();
	if (!p_mesh.GetFaceVertexIndicesAttr().Get(&face_vertex_indices)) {
		return MeshData();
	}

	auto points = PXR_NS::VtArray<PXR_NS::GfVec3f>();
	if (!p_mesh.GetPointsAttr().Get(&points)) {
		return MeshData();
	}

	return to_mesh_data(face_vertex_counts, face_vertex_indices, points);
}

MeshData to_mesh_data(const PXR_NS::UsdPrim &p_prim) {
	if (!p_prim.IsValid() || !p_prim.IsA<PXR_NS::UsdGeomMesh>()) {
		return MeshData();
	}

	return to_mesh_data(PXR_NS::UsdGeomMesh(p_prim));
}

std::vector<int32_t> triangulate(const PXR_NS::VtArray<int> &p_face_vertex_counts) {
	int triangle_count = 0;

	for (int face_count : p_face_vertex_counts) {
		if (face_count > 2) {
			triangle_count += face_count - 2;
		}
	}

	auto triangles = std::vector<int32_t>(triangle_count * 3);

	int face_index_offset = 0;
	int tri_index_offset = 0;

	for (int face_count : p_face_vertex_counts) {
		if (face_count > 2) {
			int fan_tri_count = face_count - 2;
			for (int j = 0; j < fan_tri_count; ++j) {
				triangles[tri_index_offset] = face_index_offset;
				triangles[tri_index_offset + 1] = face_index_offset + j + 1;
				triangles[tri_index_offset + 2] = face_index_offset + j + 2;
				tri_index_offset += 3;
			}
		}
		face_index_offset += face_count;
	}

	return triangles;
}

} // namespace godot_usd::translate
