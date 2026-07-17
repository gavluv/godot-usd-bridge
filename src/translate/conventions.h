#ifndef GODOT_USD_BRIDGE_TRANSLATE_CONVENTIONS_H
#define GODOT_USD_BRIDGE_TRANSLATE_CONVENTIONS_H

#include <godot_cpp/variant/transform3d.hpp>

#include <pxr/base/tf/token.h>
#include <pxr/usd/usd/common.h>

namespace godot_usd::translate {

struct StageConventions {
	double meters_per_unit;
	PXR_NS::TfToken up_axis;
	bool up_axis_unauthored;
	bool up_axis_malformed;
};

StageConventions read_stage_conventions(const PXR_NS::UsdStageWeakPtr &p_stage);

godot::Transform3D root_transform(double p_meters_per_unit, const PXR_NS::TfToken &p_up_axis);
godot::Transform3D root_transform(const StageConventions &p_conventions);
godot::Transform3D root_transform(const PXR_NS::UsdStageWeakPtr &p_stage);

} // namespace godot_usd::translate

#endif // GODOT_USD_BRIDGE_TRANSLATE_CONVENTIONS_H