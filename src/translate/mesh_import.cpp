#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include <pxr/usd/usd/prim.h>

#include "mesh_import.h"
#include "mesh_convert.h"

namespace godot_usd::translate {

static godot::String get_warn_flags_string(const uint32_t p_warn_flags) {
	if (p_warn_flags == 0) {
		return "";
	}

	auto warn_parts = godot::PackedStringArray();

	if (p_warn_flags & MESH_CONVERT_WARN_NO_GEOMETRY) {
		warn_parts.append("no geometry");
	}
	if (p_warn_flags & MESH_CONVERT_WARN_DEGENERATE_FACES) {
		warn_parts.append("degenerate faces");
	}
	if (p_warn_flags & MESH_CONVERT_WARN_UNUSED_FACE_INDICES) {
		warn_parts.append("unused face indices");
	}
	if (p_warn_flags & MESH_CONVERT_WARN_INVALID_ORIENTATION) {
		warn_parts.append("invalid orientation");
	}

	return godot::String("; ").join(warn_parts);
}

static godot::String get_channel_warn_flags_string(const uint32_t p_warn_flags) {
	if (p_warn_flags == 0) {
		return "";
	}

	auto warn_parts = godot::PackedStringArray();

	if (p_warn_flags & MESH_CONVERT_CHANNEL_WARN_INVALID_INTERPOLATION) {
		warn_parts.append("invalid interpolation mode");
	}
	if (p_warn_flags & MESH_CONVERT_CHANNEL_WARN_BAD_ELEMENT_SIZE) {
		warn_parts.append("unexpected element size");
	}
	if (p_warn_flags & MESH_CONVERT_CHANNEL_WARN_NO_AUTHORED_VALUES) {
		warn_parts.append("no authored values");
	}
	if (p_warn_flags & MESH_CONVERT_CHANNEL_WARN_UNUSED_VALUES) {
		warn_parts.append("unused values");
	}
	if (p_warn_flags & MESH_CONVERT_CHANNEL_WARN_TRUNCATED_VALUES) {
		warn_parts.append("truncated values");
	}
	if (p_warn_flags & MESH_CONVERT_CHANNEL_WARN_BAD_NORMALS_PRIMVAR) {
		warn_parts.append("malformed normals primvar");
	}
	if (p_warn_flags & MESH_CONVERT_CHANNEL_WARN_ZERO_LENGTH_NORMAL) {
		warn_parts.append("contains zero-length normal value");
	}
	if (p_warn_flags & MESH_CONVERT_CHANNEL_WARN_BAD_PRIMARY_UVS_PRIMVAR) {
		warn_parts.append("malformed primary UV set primvar");
	}

	return godot::String("; ").join(warn_parts);
}

static godot::String get_error_reason_string(const MeshConvertError p_error_reason) {
	switch (p_error_reason) {
	case MeshConvertError::NONE:
		return "None";
	case MeshConvertError::UNSET:
		return "Unset";
	case MeshConvertError::INVALID_PRIM:
		return "Invalid prim";
	case MeshConvertError::PRIM_NOT_A_MESH:
		return "Prim is not a mesh";
	case MeshConvertError::FAILED_TO_GET_FACE_VERTEX_COUNTS:
		return "Failed to get face vertex counts";
	case MeshConvertError::FAILED_TO_GET_FACE_VERTEX_INDICES:
		return "Failed to get face vertex indices";
	case MeshConvertError::FAILED_TO_GET_POINTS:
		return "Failed to get points";
	case MeshConvertError::NEGATIVE_FACE_COUNT:
		return "Negative face count";
	case MeshConvertError::CORNER_INDEX_OUT_OF_RANGE:
		return "Corner index out of range";
	case MeshConvertError::POINT_INDEX_OUT_OF_RANGE:
		return "Point index out of range";
	}
	return "Unknown error";
}

static bool channel_matches_vertex_count(size_t p_channel_size, size_t p_vertex_count) {
	return p_channel_size == p_vertex_count;
}

static godot::String get_channel_size_mismatch_error_string(size_t p_channel_size, size_t p_vertex_count) {
	return "channel array length (" + godot::String::num_uint64(p_channel_size) + ") does not match vertex count (" +
		   godot::String::num_uint64(p_vertex_count) + ") and will be ignored";
}

template <typename TPacked, typename T> static TPacked to_packed_array(const std::vector<T> &p_values) {
	TPacked packed_array;
	int64_t res = packed_array.resize(p_values.size());
	if (res != 0) {
		ERR_PRINT("Failed to resize Godot packed array. Error code: " + godot::String::num_int64(res));
		return TPacked();
	}
	if (p_values.empty()) {
		return packed_array;
	}
	T *ptr = packed_array.ptrw();
	for (size_t i = 0; i < p_values.size(); ++i) {
		ptr[i] = p_values[i];
	}
	return packed_array;
}

godot::Ref<godot::ArrayMesh> godot_mesh(const MeshData &p_mesh_data) {
	auto arrays = godot::Array();
	arrays.resize(godot::Mesh::ARRAY_MAX);

	size_t vertex_count = p_mesh_data.positions.size();

	arrays[godot::Mesh::ARRAY_VERTEX] = to_packed_array<godot::PackedVector3Array>(p_mesh_data.positions);

	// Only write optional channels if they're the same size as positions, otherwise Godot rejects the entire
	// surface; see servers/rendering/rendering_server.cpp, RenderingServer::mesh_create_surface_data_from_arrays,
	// as of Godot 4.6.3.

	if (channel_matches_vertex_count(p_mesh_data.normals.size(), vertex_count)) {
		// Tangents are deliberately not written. Godot adds a placeholder when normals are present, and real tangents
		// come with normal maps in v0.2.
		arrays[godot::Mesh::ARRAY_NORMAL] = to_packed_array<godot::PackedVector3Array>(p_mesh_data.normals);
	}

	if (channel_matches_vertex_count(p_mesh_data.uvs.size(), vertex_count)) {
		arrays[godot::Mesh::ARRAY_TEX_UV] = to_packed_array<godot::PackedVector2Array>(p_mesh_data.uvs);
	}

	arrays[godot::Mesh::ARRAY_INDEX] = to_packed_array<godot::PackedInt32Array>(p_mesh_data.triangles);

	auto mesh = godot::Ref<godot::ArrayMesh>();
	mesh.instantiate();
	mesh->add_surface_from_arrays(godot::Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

godot::MeshInstance3D *import_mesh(const PXR_NS::UsdPrim &p_prim) {
	godot::MeshInstance3D *mesh_instance = memnew(godot::MeshInstance3D);
	auto mesh_data = to_mesh_data(p_prim);
	auto warn_parts = godot::PackedStringArray();
	auto importer_err_parts = godot::PackedStringArray();
	if (mesh_data.warn_flags != 0) {
		warn_parts.append(get_warn_flags_string(mesh_data.warn_flags));
	}
	if (mesh_data.normals_warn_flags != 0) {
		warn_parts.append("[NORMALS] " + get_channel_warn_flags_string(mesh_data.normals_warn_flags));
	}
	if (!mesh_data.normals.empty() &&
			!channel_matches_vertex_count(mesh_data.normals.size(), mesh_data.positions.size())) {
		importer_err_parts.append("[NORMALS] " + get_channel_size_mismatch_error_string(
														 mesh_data.normals.size(), mesh_data.positions.size()));
	}
	if (mesh_data.uvs_warn_flags != 0) {
		warn_parts.append("[UVS] " + get_channel_warn_flags_string(mesh_data.uvs_warn_flags));
	}
	if (!mesh_data.uvs.empty() && !channel_matches_vertex_count(mesh_data.uvs.size(), mesh_data.positions.size())) {
		importer_err_parts.append(
				"[UVS] " + get_channel_size_mismatch_error_string(mesh_data.uvs.size(), mesh_data.positions.size()));
	}
	if (warn_parts.size() > 0) {
		WARN_PRINT("USD mesh conversion warnings: " + godot::String(p_prim.GetPath().GetText()) + " (" +
				   godot::String("; ").join(warn_parts) + ")");
	}
	if (importer_err_parts.size() > 0) {
		ERR_PRINT("USD mesh conversion internal errors: " + godot::String(p_prim.GetPath().GetText()) + " (" +
				  godot::String("; ").join(importer_err_parts) + ")");
	}
	if (mesh_data.is_valid()) {
		if (!mesh_data.triangles.empty()) {
			auto mesh = godot_mesh(mesh_data);
			mesh->set_name(godot::String(p_prim.GetName().GetText()));
			mesh_instance->set_mesh(mesh);
		}
	} else {
		ERR_PRINT("Failed to convert USD mesh to Godot mesh: " + godot::String(p_prim.GetPath().GetText()) + " (" +
				  get_error_reason_string(mesh_data.error_reason) + ")");
	}

	return mesh_instance;
}

} // namespace godot_usd::translate
