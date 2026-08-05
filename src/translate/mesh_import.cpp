#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include <pxr/usd/usdGeom/mesh.h>

#include "mesh_import.h"
#include "mesh_convert.h"

namespace godot_usd::translate {

godot::Ref<godot::ArrayMesh> godot_mesh(const MeshData &p_mesh_data) {
	auto arrays = godot::Array();
	arrays.resize(godot::Mesh::ARRAY_MAX);

	auto points_array = godot::PackedVector3Array();
	points_array.resize(p_mesh_data.points.size());
	for (size_t i = 0; i < p_mesh_data.points.size(); ++i) {
		points_array.set(i, p_mesh_data.points[i]);
	}
	arrays[godot::Mesh::ARRAY_VERTEX] = points_array;

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

godot::MeshInstance3D *import_mesh(const PXR_NS::UsdGeomMesh &p_usd_mesh) {
	godot::MeshInstance3D *mesh_instance = memnew(godot::MeshInstance3D);

	auto mesh_data = to_mesh_data(p_usd_mesh);
	if (mesh_data.is_valid) {
		if (mesh_data.triangles.empty()) {
			WARN_PRINT("USD mesh has no geometry: " + godot::String(p_usd_mesh.GetPrim().GetPath().GetText()));
		} else {
			auto mesh = godot_mesh(mesh_data);
			mesh->set_name(godot::String(p_usd_mesh.GetPrim().GetName().GetText()));
			mesh_instance->set_mesh(mesh);
		}
	} else {
		ERR_PRINT(
				"Failed to convert USD mesh to Godot mesh: " + godot::String(p_usd_mesh.GetPrim().GetPath().GetText()));
	}

	return mesh_instance;
}

} // namespace godot_usd::translate
