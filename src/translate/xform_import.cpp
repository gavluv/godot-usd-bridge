#include "xform_import.h"
#include "conventions.h"
#include "xform_convert.h"

#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/error_macros.hpp>

#include <pxr/usd/usd/common.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/xformCache.h>

#include <string>

namespace godot_usd::translate {

static const char *prim_path_md_key = "usd_prim_path";
static const char *prim_type_name_md_key = "usd_type_name";

static godot::Node3D *import_prim(const PXR_NS::UsdPrim &p_prim, PXR_NS::UsdGeomXformCache &p_xform_cache, const godot::Transform3D &p_root_transform) {
	if (!p_prim.IsValid()) {
		ERR_PRINT("Invalid prim");
		return nullptr;
	}

	auto node = memnew(godot::Node3D);

	bool resets_xform_stack = false;

	auto usd_matrix = p_xform_cache.GetLocalTransformation(p_prim, &resets_xform_stack);
	auto transform = to_godot_transform(usd_matrix);

	if (resets_xform_stack) {
		// apply the root transformation (upAxis, metersPerUnit) to this node's top-level transform
		node->set_transform(p_root_transform * transform);
		node->set_as_top_level(true);
	}
	else {
		node->set_transform(transform);
	}

	node->set_name(godot::String(p_prim.GetName().GetText()).validate_node_name());
	node->set_meta(prim_type_name_md_key, godot::String(p_prim.GetTypeName().GetText()));
	node->set_meta(prim_path_md_key, godot::String(p_prim.GetPath().GetText()));

	for (PXR_NS::UsdPrim child_prim : p_prim.GetChildren()) {
		auto child_node = import_prim(child_prim, p_xform_cache, p_root_transform);
		if (child_node != nullptr) {
			node->add_child(child_node);
		}
	}

	return node;
}

godot::Node3D *import_stage(const PXR_NS::UsdStageWeakPtr &p_stage) {
	if (p_stage == nullptr) {
		ERR_PRINT("Invalid stage");
		return nullptr;
	}

	PXR_NS::UsdPrim root_prim = p_stage->GetPseudoRoot();

	godot::Transform3D root_tf = root_transform(p_stage);
	auto xform_cache = PXR_NS::UsdGeomXformCache();

	auto root_node = import_prim(root_prim, xform_cache, root_tf);

	if (root_node == nullptr) {
		return nullptr;
	}

	if (!root_node->is_set_as_top_level()) {
		root_node->set_transform(root_tf * root_node->get_transform());
	}

	// try to use the stage's source filename for the root node name
	std::string root_node_name = p_stage->GetRootLayer()->GetDisplayName();
	if (root_node_name.empty()) {
		root_node_name = "_root";
	}
	root_node->set_name(godot::String(root_node_name.c_str()).validate_node_name());

	return root_node;
}

} // namespace godot_usd::translate
