#include "mesh_convert.h"

#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/templates/hashfuncs.hpp>

#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdUtils/pipeline.h>

#include <vector>
#include <utility>
#include <unordered_map>

namespace godot_usd::translate {

struct VertexKey {
	// position is keyed by index because points have no interpolation mode
	int32_t point_index;
	godot::Vector3 normal;
	godot::Vector2 uv;
};

// Using Godot hash functions and comparators with std::unordered_map instead of godot::HashMap;
// see comment on `vertex_hash_map` in `to_mesh_data()` below and ADR-006.
struct VertexKeyHasher {
	size_t operator()(const VertexKey &key) const {
		uint32_t h = godot::hash_murmur3_one_32(key.point_index);
		h = godot::hash_murmur3_one_32(godot::HashMapHasherDefault::hash(key.normal), h);
		h = godot::hash_murmur3_one_32(godot::HashMapHasherDefault::hash(key.uv), h);
		return godot::hash_fmix32(h);
	}
};

struct VertexKeyComparator {
	bool operator()(const VertexKey &p_lhs, const VertexKey &p_rhs) const {
		return p_lhs.point_index == p_rhs.point_index &&
			   godot::HashMapComparatorDefault<godot::Vector3>::compare(p_lhs.normal, p_rhs.normal) &&
			   godot::HashMapComparatorDefault<godot::Vector2>::compare(p_lhs.uv, p_rhs.uv);
	}
};

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

void flip_triangles_winding(std::vector<int32_t> &r_tri_indices) {
	// Mutates r_tri_indices in-place; swaps each triangle's slots 1 and 2 to flip the winding order.
	// We swap 1 and 2 instead of 0 and 2 because the former keeps the fan's anchor corner in slot 0 and matches
	// how Godot's glTF importer flips winding (see _parse_meshes in godot/modules/gltf/gltf_document.cpp).

	if (r_tri_indices.size() < 3) {
		return;
	}

	for (size_t i = 0; i < r_tri_indices.size() - 2; i += 3) {
		std::swap(r_tri_indices[i + 1], r_tri_indices[i + 2]);
	}
}

static void lazy_build_corner_to_face(InterpolationMode p_check_mode, const PXR_NS::VtArray<int> &p_face_vertex_counts,
		std::optional<std::vector<int32_t>> *r_corner_to_face) {
	if (!r_corner_to_face->has_value() && p_check_mode == InterpolationMode::UNIFORM) {
		*r_corner_to_face = corner_to_face(p_face_vertex_counts);
	}
}

MeshData to_mesh_data(const PXR_NS::VtArray<int> &p_face_vertex_counts,
		const PXR_NS::VtArray<int> &p_face_vertex_indices, const PXR_NS::VtArray<PXR_NS::GfVec3f> &p_points,
		bool p_flip_winding, const std::optional<MeshChannelParams<PXR_NS::GfVec3f>> &p_normals,
		const std::optional<MeshChannelParams<PXR_NS::GfVec2f>> &p_uvs) {
	auto mesh_data = MeshData();
	size_t corner_count = 0;

	mesh_data.error_reason =
			validate_topology(p_face_vertex_counts, p_face_vertex_indices.size(), &mesh_data.warn_flags, &corner_count);

	if (!mesh_data.is_valid()) {
		return mesh_data;
	}

	auto triangles = triangulate(p_face_vertex_counts);

	size_t point_count = p_points.size();
	size_t face_count = p_face_vertex_counts.size();

	std::optional<std::vector<int32_t>> corner_to_face_map = std::nullopt;

	// normals
	bool normals_valid = false;
	std::vector<godot::Vector3> normals_values;
	std::vector<int32_t> normals_indices;

	if (p_normals.has_value() && validate_channel(p_normals->mode, p_normals->values.size(), point_count, face_count,
										 corner_count, &mesh_data.normals_warn_flags)) {
		lazy_build_corner_to_face(p_normals->mode, p_face_vertex_counts, &corner_to_face_map);
		normals_indices = sample_value_indices(p_normals->mode, triangles, corner_to_face_map, p_face_vertex_indices);
		normals_values = std::vector<godot::Vector3>(p_normals->values.size());
		for (size_t i = 0; i < normals_values.size(); ++i) {
			if (p_normals->values[i].GetLengthSq() == 0.0f) {
				mesh_data.normals_warn_flags |= MESH_CONVERT_CHANNEL_WARN_ZERO_LENGTH_NORMAL;
				normals_values[i] = godot::Vector3(0, 1, 0);
			} else {
				normals_values[i] =
						godot::Vector3(p_normals->values[i][0], p_normals->values[i][1], p_normals->values[i][2])
								.normalized();
			}
		}
		normals_valid = true;
	}

	// uvs
	bool uvs_valid = false;
	std::vector<godot::Vector2> uvs_values;
	std::vector<int32_t> uvs_indices;

	if (p_uvs.has_value() && validate_channel(p_uvs->mode, p_uvs->values.size(), point_count, face_count, corner_count,
									 &mesh_data.uvs_warn_flags)) {
		lazy_build_corner_to_face(p_uvs.value().mode, p_face_vertex_counts, &corner_to_face_map);
		uvs_indices = sample_value_indices(p_uvs->mode, triangles, corner_to_face_map, p_face_vertex_indices);
		uvs_values = std::vector<godot::Vector2>(p_uvs->values.size());
		for (size_t i = 0; i < uvs_values.size(); ++i) {
			uvs_values[i] = godot::Vector2(p_uvs->values[i][0], 1.0f - p_uvs->values[i][1]);
		}
		uvs_valid = true;
	}

	mesh_data.error_reason = map_corners_to_points(triangles, p_face_vertex_indices, point_count);

	if (!mesh_data.is_valid()) {
		return mesh_data;
	}

	// split vertices and dedup

	// Using std::unordered_map instead of godot::HashMap because the latter compiles fine but segfaults
	// the test exe; godot-cpp's templates/ containers allocate via memnew -> Memory::alloc_static ->
	// gdextension_interface::mem_alloc, which is a null function pointer without the Godot engine present.
	// Godot's hash functions and comparators are safe without the engine, handle -0.0 and NaN equality,
	// and should stay paired; see ADR-006.
	auto vertex_hash_map = std::unordered_map<VertexKey, int32_t, VertexKeyHasher, VertexKeyComparator>();
	vertex_hash_map.reserve(point_count);
	auto seen_vertices = std::vector<uint8_t>(point_count, false);
	int32_t next_append_index = point_count;
	size_t used_point_count = 0;

	mesh_data.positions = std::vector<godot::Vector3>(point_count);

	if (normals_valid) {
		mesh_data.normals = std::vector<godot::Vector3>(point_count, godot::Vector3(0, 1, 0));
	}

	if (uvs_valid) {
		mesh_data.uvs = std::vector<godot::Vector2>(point_count);
	}

	for (size_t i = 0; i < triangles.size(); ++i) {
		auto key = VertexKey{triangles[i], normals_valid ? normals_values[normals_indices[i]] : godot::Vector3(),
				uvs_valid ? uvs_values[uvs_indices[i]] : godot::Vector2()};

		int32_t tri = triangles[i];

		auto [it, inserted] = vertex_hash_map.try_emplace(key, -1);

		if (!inserted) {
			// vertex already in vertex_hash_map -> remap current tri index
			tri = it->second;
		} else if (seen_vertices[tri]) {
			// unique vertex, point index already taken -> append
			mesh_data.positions.push_back(godot::Vector3(p_points[tri][0], p_points[tri][1], p_points[tri][2]));
			if (normals_valid) {
				mesh_data.normals.push_back(normals_values[normals_indices[i]]);
			}
			if (uvs_valid) {
				mesh_data.uvs.push_back(uvs_values[uvs_indices[i]]);
			}

			tri = next_append_index;
			it->second = tri;
			next_append_index++;
		} else {
			// unique vertex, point not seen -> insert at triangles[i]
			mesh_data.positions[tri] = godot::Vector3(p_points[tri][0], p_points[tri][1], p_points[tri][2]);
			if (normals_valid) {
				mesh_data.normals[tri] = normals_values[normals_indices[i]];
			}
			if (uvs_valid) {
				mesh_data.uvs[tri] = uvs_values[uvs_indices[i]];
			}

			seen_vertices[tri] = true;
			used_point_count++;
			it->second = tri;
		}

		triangles[i] = tri;
	}

	if (used_point_count != point_count) {
		// remove vertices in range [0, point_count) not referenced by any triangle

		auto remap_tris = std::vector<int32_t>(mesh_data.positions.size());

		size_t new_i = 0;
		for (size_t old_i = 0; old_i < mesh_data.positions.size(); ++old_i) {
			if (old_i >= point_count || seen_vertices[old_i]) {
				remap_tris[old_i] = new_i;
				if (new_i != old_i) {
					mesh_data.positions[new_i] = mesh_data.positions[old_i];
					if (normals_valid) {
						mesh_data.normals[new_i] = mesh_data.normals[old_i];
					}
					if (uvs_valid) {
						mesh_data.uvs[new_i] = mesh_data.uvs[old_i];
					}
				}
				new_i++;
			} else {
				remap_tris[old_i] = -1;
			}
		}

		mesh_data.positions.resize(new_i);
		if (normals_valid) {
			mesh_data.normals.resize(new_i);
		}
		if (uvs_valid) {
			mesh_data.uvs.resize(new_i);
		}

		for (size_t i = 0; i < triangles.size(); ++i) {
			triangles[i] = remap_tris[triangles[i]];
		}
	}

	// Winding flip has to come after the dedup walk, because the dedup walk numbers split vertices in visit order.
	// After dedup, the flip only permutes indices within each triangle, so the vertex buffers are identical for both
	// orientations.
	if (p_flip_winding) {
		flip_triangles_winding(triangles);
	}

	mesh_data.triangles = std::move(triangles);

	if (mesh_data.triangles.empty()) {
		mesh_data.warn_flags |= MESH_CONVERT_WARN_NO_GEOMETRY;
	}

	return mesh_data;
}

MeshData to_mesh_data(const PXR_NS::UsdPrim &p_prim) {
	if (!p_prim.IsValid()) {
		return MeshData::from_error(MeshConvertError::INVALID_PRIM);
	}

	if (!p_prim.IsA<PXR_NS::UsdGeomMesh>()) {
		return MeshData::from_error(MeshConvertError::PRIM_NOT_A_MESH);
	}

	PXR_NS::UsdGeomMesh mesh = PXR_NS::UsdGeomMesh(p_prim);

	auto face_vertex_counts = PXR_NS::VtArray<int>();
	if (!mesh.GetFaceVertexCountsAttr().Get(&face_vertex_counts)) {
		return MeshData::from_error(MeshConvertError::FAILED_TO_GET_FACE_VERTEX_COUNTS);
	}

	auto face_vertex_indices = PXR_NS::VtArray<int>();
	if (!mesh.GetFaceVertexIndicesAttr().Get(&face_vertex_indices)) {
		return MeshData::from_error(MeshConvertError::FAILED_TO_GET_FACE_VERTEX_INDICES);
	}

	auto points = PXR_NS::VtArray<PXR_NS::GfVec3f>();
	if (!mesh.GetPointsAttr().Get(&points)) {
		return MeshData::from_error(MeshConvertError::FAILED_TO_GET_POINTS);
	}

	auto primvars_api = PXR_NS::UsdGeomPrimvarsAPI(p_prim);

	// normals
	std::optional<MeshChannelParams<PXR_NS::GfVec3f>> normals = std::nullopt;
	std::optional<InterpolationMode> normals_mode = std::nullopt;
	auto normals_values = PXR_NS::VtArray<PXR_NS::GfVec3f>();
	uint32_t normals_warnings = 0;

	if (primvars_api.HasPrimvar(PXR_NS::UsdGeomTokens->normals)) {
		auto normals_primvar = primvars_api.GetPrimvar(PXR_NS::UsdGeomTokens->normals);
		normals_mode = get_interpolation_mode(normals_primvar.GetInterpolation());
		if (!normals_mode.has_value()) {
			normals_warnings |= MESH_CONVERT_CHANNEL_WARN_INVALID_INTERPOLATION;
		} else if (normals_primvar.GetElementSize() != 1) {
			normals_warnings |= MESH_CONVERT_CHANNEL_WARN_BAD_ELEMENT_SIZE;
		} else if (normals_primvar.ComputeFlattened(&normals_values)) {
			normals = MeshChannelParams<PXR_NS::GfVec3f>{normals_mode.value(), normals_values};
		} else {
			normals_warnings |= MESH_CONVERT_CHANNEL_WARN_NO_AUTHORED_VALUES;
		}
	}

	if (!normals.has_value()) {
		normals_mode = get_interpolation_mode(mesh.GetNormalsInterpolation());
		auto normals_attr = mesh.GetNormalsAttr();
		if (!normals_mode.has_value()) {
			normals_warnings |= MESH_CONVERT_CHANNEL_WARN_INVALID_INTERPOLATION;
		} else if (normals_attr.Get(&normals_values)) {
			if (normals_warnings != 0) {
				// primvars:normals exists but we couldn't use it; we have a usable normals attribute to fall back to
				// -> clear warnings for the primvar and set a unique flag
				normals_warnings = MESH_CONVERT_CHANNEL_WARN_BAD_NORMALS_PRIMVAR;
			}
			normals = MeshChannelParams<PXR_NS::GfVec3f>{normals_mode.value(), normals_values};
		} else if (!normals_attr.IsAuthored()) {
			// no authored normals attribute -> valid, don't warn
		} else if (normals_warnings == 0) {
			// primvars:normals warnings take precedence, so only warn about normals if the primvar had none
			normals_warnings |= MESH_CONVERT_CHANNEL_WARN_NO_AUTHORED_VALUES;
		}
	}

	// uvs
	std::optional<MeshChannelParams<PXR_NS::GfVec2f>> uvs = std::nullopt;
	uint32_t uvs_warnings = 0;
	bool has_primary_uvs_primvar = primvars_api.HasPrimvar(PXR_NS::UsdUtilsGetPrimaryUVSetName());

	if (has_primary_uvs_primvar) {
		auto uvs_primvar = primvars_api.GetPrimvar(PXR_NS::UsdUtilsGetPrimaryUVSetName());
		auto uvs_mode = get_interpolation_mode(uvs_primvar.GetInterpolation());
		auto uvs_values = PXR_NS::VtArray<PXR_NS::GfVec2f>();
		if (!uvs_mode.has_value()) {
			uvs_warnings |= MESH_CONVERT_CHANNEL_WARN_INVALID_INTERPOLATION;
		} else if (uvs_primvar.GetElementSize() != 1) {
			uvs_warnings |= MESH_CONVERT_CHANNEL_WARN_BAD_ELEMENT_SIZE;
		} else if (uvs_primvar.ComputeFlattened(&uvs_values)) {
			uvs = MeshChannelParams<PXR_NS::GfVec2f>{uvs_mode.value(), uvs_values};
		} else {
			uvs_warnings |= MESH_CONVERT_CHANNEL_WARN_NO_AUTHORED_VALUES;
		}
	}

	if (!uvs.has_value()) {
		// if there's no primvar with the primary UV set name ('st'), or primvars:st was not usable, fall back to any
		// texCoord2f[]-typed primvar
		for (const auto &primvar : primvars_api.GetPrimvarsWithValues()) {
			if (primvar.GetPrimvarName() == PXR_NS::UsdUtilsGetPrimaryUVSetName()) {
				// already checked this primvar and failed
				continue;
			}
			if (primvar.GetTypeName() == PXR_NS::SdfValueTypeNames->TexCoord2fArray) {
				auto uvs_mode = get_interpolation_mode(primvar.GetInterpolation());
				auto uvs_values = PXR_NS::VtArray<PXR_NS::GfVec2f>();
				if (!uvs_mode.has_value()) {
					uvs_warnings |= MESH_CONVERT_CHANNEL_WARN_INVALID_INTERPOLATION;
				} else if (primvar.GetElementSize() != 1) {
					uvs_warnings |= MESH_CONVERT_CHANNEL_WARN_BAD_ELEMENT_SIZE;
				} else if (primvar.ComputeFlattened(&uvs_values)) {
					// we have a usable texCoord2f[] primvar to fall back to -> clear warnings from previous failures
					// and set a unique flag if primvars:st exists but was unusable
					uvs_warnings = has_primary_uvs_primvar ? MESH_CONVERT_CHANNEL_WARN_BAD_PRIMARY_UVS_PRIMVAR : 0;
					uvs = MeshChannelParams<PXR_NS::GfVec2f>{uvs_mode.value(), uvs_values};
					break;
				} else {
					uvs_warnings |= MESH_CONVERT_CHANNEL_WARN_NO_AUTHORED_VALUES;
				}
			}
		}
	}

	// Godot's front faces are clockwise and USD's default rightHanded orientation is counter-clockwise,
	// so if this mesh is right handed we need to flip its winding order.
	// TODO: read orientation via UsdGeomMesh.GetOrientationAttr()
	const bool flip_winding = true;

	auto mesh_data = to_mesh_data(face_vertex_counts, face_vertex_indices, points, flip_winding, normals, uvs);

	mesh_data.normals_warn_flags |= normals_warnings;
	mesh_data.uvs_warn_flags |= uvs_warnings;

	return mesh_data;
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

bool validate_channel(InterpolationMode p_interpolation_mode, size_t p_value_count, size_t p_point_count,
		size_t p_face_count, size_t p_corner_count, uint32_t *r_channel_warn_flags) {
	size_t expected_count = expected_value_count(p_interpolation_mode, p_point_count, p_face_count, p_corner_count);

	if (p_value_count > expected_count) {
		if (r_channel_warn_flags) {
			*r_channel_warn_flags |= MESH_CONVERT_CHANNEL_WARN_UNUSED_VALUES;
		}
		return true;
	} else if (p_value_count < expected_count) {
		if (r_channel_warn_flags) {
			*r_channel_warn_flags |= MESH_CONVERT_CHANNEL_WARN_TRUNCATED_VALUES;
		}
		return false;
	}

	return true;
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
