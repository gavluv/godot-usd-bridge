#ifndef GODOT_USD_BRIDGE_TRANSLATE_MESH_CONVERT_H
#define GODOT_USD_BRIDGE_TRANSLATE_MESH_CONVERT_H

#include <godot_cpp/variant/vector3.hpp>

#include <pxr/usd/usd/common.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/base/gf/vec3f.h>

#include <vector>

namespace godot_usd::translate {

// Mesh conversion sets error_reason in the pure layer, to be reported on the engine side; can't use the Godot-bridged
// ERR_PRINT or WARN_PRINT macros without crashing the test exe.

// An error is terminal and singular, so it's a scoped discriminant. MeshData.error_reason defaults to UNSET for
// fail-closed defaulting; error_reason must be explicitly set to NONE or the object is invalid.
enum class MeshConvertError {
	NONE = 0,
	UNSET = 1,
	INVALID_SCHEMA_OBJ,
	INVALID_PRIM,
	PRIM_NOT_A_MESH,
	FAILED_TO_GET_FACE_VERTEX_COUNTS,
	FAILED_TO_GET_FACE_VERTEX_INDICES,
	FAILED_TO_GET_POINTS,
	NEGATIVE_FACE_COUNT,
	CORNER_INDEX_OUT_OF_RANGE,
	POINT_INDEX_OUT_OF_RANGE,
};

struct MeshData {
	MeshConvertError error_reason = MeshConvertError::UNSET;
	bool is_valid() const { return error_reason == MeshConvertError::NONE; }
	std::vector<godot::Vector3> positions;
	std::vector<int32_t> triangles;

	static MeshData from_error(MeshConvertError p_error_reason) {
		MeshData data;
		data.error_reason = p_error_reason;
		return data;
	}
};

MeshConvertError map_corners_to_points(
		std::vector<int32_t> &r_tri_indices, const PXR_NS::VtArray<int> &p_face_vertex_indices, size_t p_point_count);

MeshData to_mesh_data(const PXR_NS::VtArray<int> &p_face_vertex_counts,
		const PXR_NS::VtArray<int> &p_face_vertex_indices, const PXR_NS::VtArray<PXR_NS::GfVec3f> &p_points);

MeshData to_mesh_data(const PXR_NS::UsdGeomMesh &p_mesh);

MeshData to_mesh_data(const PXR_NS::UsdPrim &p_prim);

std::vector<int32_t> triangulate(const PXR_NS::VtArray<int> &p_face_vertex_counts);

} // namespace godot_usd::translate

#endif // GODOT_USD_BRIDGE_TRANSLATE_MESH_CONVERT_H
