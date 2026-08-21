#ifndef GODOT_USD_BRIDGE_TRANSLATE_MESH_CONVERT_H
#define GODOT_USD_BRIDGE_TRANSLATE_MESH_CONVERT_H

#include <godot_cpp/variant/vector3.hpp>

#include <pxr/usd/usd/common.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>

#include <vector>
#include <optional>

namespace godot_usd::translate {

// Mesh conversion captures warnings in MeshData.warn_flags and sets error_reason in the pure layer, to be reported on
// the engine side; can't use the Godot-bridged ERR_PRINT or WARN_PRINT macros without crashing the test exe.

// Warnings accumulate, so they're an unscoped bitmask, prefixed with MESH_CONVERT_WARN_* because the enumerators land
// in the enclosing namespace.
enum MeshConvertWarning {
	MESH_CONVERT_WARN_NO_GEOMETRY = 1 << 0,
	MESH_CONVERT_WARN_DEGENERATE_FACES = 1 << 1,
	MESH_CONVERT_WARN_UNUSED_FACE_INDICES = 1 << 2,
};

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

enum class InterpolationMode {
	CONSTANT,
	UNIFORM,
	VARYING,
	VERTEX,
	FACE_VARYING,
};

struct MeshData {
	uint32_t warn_flags = 0;
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

MeshConvertError validate_topology(const PXR_NS::VtArray<int> &p_face_vertex_counts, size_t p_index_count,
		uint32_t *r_warn_flags = nullptr, size_t *r_corner_count = nullptr);

std::vector<int32_t> triangulate(const PXR_NS::VtArray<int> &p_face_vertex_counts);

std::vector<int32_t> corner_to_face(const PXR_NS::VtArray<int> &p_face_vertex_counts);

std::optional<InterpolationMode> get_interpolation_mode(const PXR_NS::TfToken &p_interpolation_token);

size_t expected_value_count(
		InterpolationMode p_interpolation_mode, size_t p_point_count, size_t p_face_count, size_t p_corner_count);

std::vector<int32_t> sample_value_indices(InterpolationMode p_interpolation_mode,
		const std::vector<int32_t> &p_tri_indices, const std::optional<std::vector<int32_t>> &p_corner_to_face,
		const PXR_NS::VtArray<int> &p_face_vertex_indices);

} // namespace godot_usd::translate

#endif // GODOT_USD_BRIDGE_TRANSLATE_MESH_CONVERT_H
