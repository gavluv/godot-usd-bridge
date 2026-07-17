#include "conventions.h"

#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/core/math_defs.hpp>

#include <pxr/base/tf/token.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usd/stage.h>

using namespace godot;

namespace godot_usd::translate {

StageConventions read_stage_conventions(const PXR_NS::UsdStageWeakPtr &p_stage){
	StageConventions conventions;
	conventions.meters_per_unit = PXR_NS::UsdGeomGetStageMetersPerUnit(p_stage);
	conventions.up_axis = PXR_NS::UsdGeomGetStageUpAxis(p_stage);
	conventions.up_axis_unauthored = false;
	conventions.up_axis_malformed = false;

	if (!p_stage->HasAuthoredMetadata(PXR_NS::UsdGeomTokens->upAxis)){
		conventions.up_axis_unauthored = true;
		// up axis will be the site-configurable fallback (UsdGeomGetFallbackUpAxis()), defaults to Y-up
	}
	else if (conventions.up_axis != PXR_NS::UsdGeomTokens->y && conventions.up_axis != PXR_NS::UsdGeomTokens->z){
		conventions.up_axis_malformed = true;
		conventions.up_axis = PXR_NS::UsdGeomGetFallbackUpAxis();
	}

	return conventions;
}

Transform3D root_transform(double p_meters_per_unit, const PXR_NS::TfToken &p_up_axis){
	auto transform = Transform3D(Basis::from_scale(Vector3(p_meters_per_unit, p_meters_per_unit, p_meters_per_unit)));

	if (p_up_axis == PXR_NS::UsdGeomTokens->z){
		transform = transform.rotated(Vector3(1, 0, 0), -Math_PI / 2.0);
	}

	return transform;
}

Transform3D root_transform(const StageConventions &p_conventions){
	return root_transform(p_conventions.meters_per_unit, p_conventions.up_axis);
}

Transform3D root_transform(const PXR_NS::UsdStageWeakPtr &p_stage){
	return root_transform(read_stage_conventions(p_stage));
}

} // namespace godot_usd::translate