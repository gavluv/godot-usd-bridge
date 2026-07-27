#ifndef GODOT_USD_BRIDGE_TRANSLATE_XFORM_IMPORT_H
#define GODOT_USD_BRIDGE_TRANSLATE_XFORM_IMPORT_H

#include <godot_cpp/classes/node3d.hpp>

#include <pxr/usd/usd/common.h>

namespace godot_usd::translate {

godot::Node3D *import_stage(const PXR_NS::UsdStageWeakPtr &p_stage);

} // namespace godot_usd::translate

#endif // GODOT_USD_BRIDGE_TRANSLATE_XFORM_IMPORT_H
