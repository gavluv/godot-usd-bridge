#include "mesh_convert.h"

#include <godot_cpp/variant/vector3.hpp>

#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <vector>
#include <utility>

namespace godot_usd::translate {

MeshConvertError map_corners_to_points(
		std::vector<int32_t> &r_tri_indices, const PXR_NS::VtArray<int> &p_face_vertex_indices, size_t p_point_count) {
	const int64_t index_count = static_cast<int64_t>(p_face_vertex_indices.size());
	const int64_t point_count = static_cast<int64_t>(p_point_count);

	// mutates r_tri_indices in-place; r_tri_indices holds corner offsets entering, point indices leaving the loop

	for (size_t i = 0; i < r_tri_indices.size(); ++i) {
		if (r_tri_indices[i] >= index_count) {
			return MeshConvertError::CORNER_INDEX_OUT_OF_RANGE;
		}
		const int point_index = p_face_vertex_indices[r_tri_indices[i]];
		if (point_index < 0 || point_index >= point_count) {
			return MeshConvertError::POINT_INDEX_OUT_OF_RANGE;
		}
		r_tri_indices[i] = point_index;
	}

	return MeshConvertError::NONE;
}

MeshData to_mesh_data(const PXR_NS::VtArray<int> &p_face_vertex_counts,
		const PXR_NS::VtArray<int> &p_face_vertex_indices, const PXR_NS::VtArray<PXR_NS::GfVec3f> &p_points) {
	auto mesh_data = MeshData();

	mesh_data.error_reason =
			validate_topology(p_face_vertex_counts, p_face_vertex_indices.size(), &mesh_data.warn_flags);

	if (!mesh_data.is_valid()) {
		return mesh_data;
	}

	auto triangles = triangulate(p_face_vertex_counts);

	size_t point_count = p_points.size();

	mesh_data.error_reason = map_corners_to_points(triangles, p_face_vertex_indices, point_count);

	if (!mesh_data.is_valid()) {
		return mesh_data;
	}

	auto positions = std::vector<godot::Vector3>(point_count);

	for (size_t i = 0; i < p_points.size(); i++) {
		positions[i] = godot::Vector3(p_points[i][0], p_points[i][1], p_points[i][2]);
	}

	mesh_data.positions = std::move(positions);
	mesh_data.triangles = std::move(triangles);

	if (mesh_data.triangles.empty()) {
		mesh_data.warn_flags |= MESH_CONVERT_WARN_NO_GEOMETRY;
	}

	return mesh_data;
}

MeshData to_mesh_data(const PXR_NS::UsdGeomMesh &p_mesh) {
	if (!static_cast<bool>(p_mesh)) {
		return MeshData::from_error(MeshConvertError::INVALID_SCHEMA_OBJ);
	}

	auto face_vertex_counts = PXR_NS::VtArray<int>();
	if (!p_mesh.GetFaceVertexCountsAttr().Get(&face_vertex_counts)) {
		return MeshData::from_error(MeshConvertError::FAILED_TO_GET_FACE_VERTEX_COUNTS);
	}

	auto face_vertex_indices = PXR_NS::VtArray<int>();
	if (!p_mesh.GetFaceVertexIndicesAttr().Get(&face_vertex_indices)) {
		return MeshData::from_error(MeshConvertError::FAILED_TO_GET_FACE_VERTEX_INDICES);
	}

	auto points = PXR_NS::VtArray<PXR_NS::GfVec3f>();
	if (!p_mesh.GetPointsAttr().Get(&points)) {
		return MeshData::from_error(MeshConvertError::FAILED_TO_GET_POINTS);
	}

	return to_mesh_data(face_vertex_counts, face_vertex_indices, points);
}

MeshData to_mesh_data(const PXR_NS::UsdPrim &p_prim) {
	if (!p_prim.IsValid()) {
		return MeshData::from_error(MeshConvertError::INVALID_PRIM);
	}

	if (!p_prim.IsA<PXR_NS::UsdGeomMesh>()) {
		return MeshData::from_error(MeshConvertError::PRIM_NOT_A_MESH);
	}

	return to_mesh_data(PXR_NS::UsdGeomMesh(p_prim));
}

// Topology helper functions to keep triangulate() and validate_topology() in agreement about what a face consumes and
// produces.

static int get_corners_consumed(int p_face_count) {
	if (p_face_count < 0) {
		return 0;
	}
	return p_face_count;
}

static int get_triangles_produced(int p_face_count) {
	if (p_face_count < 3) {
		return 0;
	}
	return p_face_count - 2;
}

MeshConvertError validate_topology(const PXR_NS::VtArray<int> &p_face_vertex_counts, size_t p_index_count,
		uint32_t *r_warn_flags, size_t *r_corner_count) {
	size_t face_index_offset = 0;

	for (int face_count : p_face_vertex_counts) {
		if (face_count < 0) {
			// A negative face count is invalid topology; the intended index offsets for all subsequent faces are
			// unrecoverable. Incorrectly guessing at the partition, e.g., by assuming a negative-count face consumes 0
			// indices, produces a mesh with indices that are all in range but silently wrong, so we fail the whole mesh
			// rather than skipping the face with a warning.
			return MeshConvertError::NEGATIVE_FACE_COUNT;
		}
		if (get_triangles_produced(face_count) == 0) {
			if (r_warn_flags) {
				*r_warn_flags |= MESH_CONVERT_WARN_DEGENERATE_FACES;
			}
		}

		face_index_offset += get_corners_consumed(face_count);
	}

	if (face_index_offset < p_index_count && r_warn_flags) {
		*r_warn_flags |= MESH_CONVERT_WARN_UNUSED_FACE_INDICES;
	}

	if (face_index_offset > p_index_count) {
		return MeshConvertError::CORNER_INDEX_OUT_OF_RANGE;
	}

	if (r_corner_count) {
		*r_corner_count = face_index_offset;
	}

	return MeshConvertError::NONE;
}

std::vector<int32_t> corner_to_face(const PXR_NS::VtArray<int> &p_face_vertex_counts) {
	size_t corner_count = 0;
	for (int face_count : p_face_vertex_counts) {
		corner_count += get_corners_consumed(face_count);
	}

	auto corner_to_face_indices = std::vector<int32_t>(corner_count);
	int32_t num_faces = static_cast<int32_t>(p_face_vertex_counts.size());

	int32_t corner_index = 0;
	int32_t face_index = 0;

	while (face_index < num_faces) {
		int face_count = get_corners_consumed(p_face_vertex_counts[face_index]);
		for (int i = 0; i < face_count; ++i) {
			corner_to_face_indices[corner_index] = face_index;
			corner_index++;
		}
		face_index++;
	}

	return corner_to_face_indices;
}

std::vector<int32_t> triangulate(const PXR_NS::VtArray<int> &p_face_vertex_counts) {
	int triangle_count = 0;

	for (int face_count : p_face_vertex_counts) {
		triangle_count += get_triangles_produced(face_count);
	}

	auto triangles = std::vector<int32_t>(triangle_count * 3);

	int face_index_offset = 0;
	int tri_index_offset = 0;

	for (int face_count : p_face_vertex_counts) {
		int fan_tri_count = get_triangles_produced(face_count);
		if (fan_tri_count > 0) {
			for (int j = 0; j < fan_tri_count; ++j) {
				triangles[tri_index_offset] = face_index_offset;
				triangles[tri_index_offset + 1] = face_index_offset + j + 1;
				triangles[tri_index_offset + 2] = face_index_offset + j + 2;
				tri_index_offset += 3;
			}
		}

		face_index_offset += get_corners_consumed(face_count);
	}

	return triangles;
}

std::optional<InterpolationMode> get_interpolation_mode(const PXR_NS::TfToken &p_interpolation_token) {
	if (p_interpolation_token == PXR_NS::UsdGeomTokens->constant) {
		return InterpolationMode::CONSTANT;
	}
	if (p_interpolation_token == PXR_NS::UsdGeomTokens->uniform) {
		return InterpolationMode::UNIFORM;
	}
	if (p_interpolation_token == PXR_NS::UsdGeomTokens->varying) {
		return InterpolationMode::VARYING;
	}
	if (p_interpolation_token == PXR_NS::UsdGeomTokens->vertex) {
		return InterpolationMode::VERTEX;
	}
	if (p_interpolation_token == PXR_NS::UsdGeomTokens->faceVarying) {
		return InterpolationMode::FACE_VARYING;
	}
	return std::nullopt;
}

size_t expected_value_count(
		InterpolationMode p_interpolation_mode, size_t p_point_count, size_t p_face_count, size_t p_corner_count) {
	switch (p_interpolation_mode) {
	case InterpolationMode::CONSTANT:
		return 1;
	case InterpolationMode::UNIFORM:
		return p_face_count;
	case InterpolationMode::VARYING:
		return p_point_count;
	case InterpolationMode::VERTEX:
		return p_point_count;
	case InterpolationMode::FACE_VARYING:
		return p_corner_count;
	}
	// Unreachable; prevents compiler warning. Returns SIZE_MAX so that values.size() >= expected_value_count(...)
	// checks fail if p_interpolation_mode is invalid.
	return size_t(-1);
}

std::vector<int32_t> sample_value_indices(InterpolationMode p_interpolation_mode,
		const std::vector<int32_t> &p_tri_indices, const std::optional<std::vector<int32_t>> &p_corner_to_face,
		const PXR_NS::VtArray<int> &p_face_vertex_indices) {
	std::vector<int32_t> value_indices = std::vector<int32_t>(p_tri_indices.size());

	switch (p_interpolation_mode) {
	case InterpolationMode::CONSTANT:
		// Don't need to do anything; value_indices is already all 0's
		break;
	case InterpolationMode::UNIFORM:
		// p_corner_to_face.value() throws std::bad_optional_access if the caller fails to supply it
		for (size_t i = 0; i < p_tri_indices.size(); ++i) {
			value_indices[i] = p_corner_to_face.value()[p_tri_indices[i]];
		}
		break;
	case InterpolationMode::VARYING:
	case InterpolationMode::VERTEX:
		for (size_t i = 0; i < p_tri_indices.size(); ++i) {
			value_indices[i] = p_face_vertex_indices[p_tri_indices[i]];
		}
		break;
	case InterpolationMode::FACE_VARYING:
		for (size_t i = 0; i < p_tri_indices.size(); ++i) {
			value_indices[i] = p_tri_indices[i];
		}
		break;
	}

	return value_indices;
}

} // namespace godot_usd::translate
