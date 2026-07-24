#ifndef GODOT_USD_BRIDGE_TRANSLATE_XFORM_CONVERT_H
#define GODOT_USD_BRIDGE_TRANSLATE_XFORM_CONVERT_H

#include <godot_cpp/variant/transform3d.hpp>

#include <pxr/usd/usd/common.h>
#include <pxr/base/gf/matrix4d.h>

namespace godot_usd::translate {

godot::Transform3D to_godot_transform(const PXR_NS::GfMatrix4d &p_usd_matrix);

} // namespace godot_usd::translate

#endif // GODOT_USD_BRIDGE_TRANSLATE_XFORM_CONVERT_H
