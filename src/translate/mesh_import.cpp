#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
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

godot::Ref<godot::ArrayMesh> godot_mesh(const MeshData &p_mesh_data) {
	auto arrays = godot::Array();
	arrays.resize(godot::Mesh::ARRAY_MAX);

	auto positions_array = godot::PackedVector3Array();
	positions_array.resize(p_mesh_data.positions.size());
	for (size_t i = 0; i < p_mesh_data.positions.size(); ++i) {
		positions_array.set(i, p_mesh_data.positions[i]);
	}
	arrays[godot::Mesh::ARRAY_VERTEX] = positions_array;

	auto triangles_array = godot::PackedInt32Array();
	triangles_array.resize(p_mesh_data.triangles.size());
	for (size_t i = 0; i < p_mesh_data.triangles.size(); ++i) {
		triangles_array.set(i, p_mesh_data.triangles[i]);
	}
	arrays[godot::Mesh::ARRAY_INDEX] = triangles_array;

	auto mesh = godot::Ref<godot::ArrayMesh>();
	mesh.instantiate();
	mesh->add_surface_from_arrays(godot::Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

godot::MeshInstance3D *import_mesh(const PXR_NS::UsdPrim &p_prim) {
	godot::MeshInstance3D *mesh_instance = memnew(godot::MeshInstance3D);
	auto mesh_data = to_mesh_data(p_prim);
	auto warn_parts = godot::PackedStringArray();
	if (mesh_data.warn_flags != 0) {
		warn_parts.append(get_warn_flags_string(mesh_data.warn_flags));
	}
	if (mesh_data.normals_warn_flags != 0) {
		warn_parts.append("[NORMALS] " + get_channel_warn_flags_string(mesh_data.normals_warn_flags));
	}
	if (mesh_data.uvs_warn_flags != 0) {
		warn_parts.append("[UVS] " + get_channel_warn_flags_string(mesh_data.uvs_warn_flags));
	}
	if (warn_parts.size() > 0) {
		WARN_PRINT("USD mesh conversion warnings: " + godot::String(p_prim.GetPath().GetText()) + " (" +
				   godot::String("; ").join(warn_parts) + ")");
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
