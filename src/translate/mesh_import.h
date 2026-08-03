#ifndef GODOT_USD_BRIDGE_TRANSLATE_MESH_IMPORT_H
#define GODOT_USD_BRIDGE_TRANSLATE_MESH_IMPORT_H

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>

#include <pxr/usd/usdGeom/mesh.h>

#include "mesh_convert.h"

namespace godot_usd::translate {

godot::Ref<godot::ArrayMesh> godot_mesh(const MeshData &p_mesh_data);
godot::MeshInstance3D *import_mesh(const PXR_NS::UsdGeomMesh &p_usd_mesh);

} // namespace godot_usd::translate

#endif // GODOT_USD_BRIDGE_TRANSLATE_MESH_IMPORT_H
