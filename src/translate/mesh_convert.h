#ifndef GODOT_USD_BRIDGE_TRANSLATE_MESH_CONVERT_H
#define GODOT_USD_BRIDGE_TRANSLATE_MESH_CONVERT_H

#include <godot_cpp/variant/vector3.hpp>

#include <pxr/usd/usd/common.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/base/gf/vec3f.h>

#include <vector>

namespace godot_usd::translate {

struct MeshData {
	bool is_valid = false;
	std::vector<godot::Vector3> points;
	std::vector<int32_t> triangles;
};

bool map_corners_to_points(std::vector<int32_t> &r_tri_indices, const PXR_NS::VtArray<int> &p_face_vertex_indices,
	size_t p_point_count);

MeshData to_mesh_data(const PXR_NS::VtArray<int> &p_face_vertex_counts, const PXR_NS::VtArray<int> &p_face_vertex_indices,
	const PXR_NS::VtArray<PXR_NS::GfVec3f> &p_points);

MeshData to_mesh_data(const PXR_NS::UsdGeomMesh &p_mesh);

MeshData to_mesh_data(const PXR_NS::UsdPrim &p_prim);

std::vector<int32_t> triangulate(const PXR_NS::VtArray<int> &p_face_vertex_counts);

} // namespace godot_usd::translate

#endif // GODOT_USD_BRIDGE_TRANSLATE_MESH_CONVERT_H
