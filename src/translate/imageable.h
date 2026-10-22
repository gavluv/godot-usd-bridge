#ifndef GODOT_USD_BRIDGE_TRANSLATE_IMAGEABLE_H
#define GODOT_USD_BRIDGE_TRANSLATE_IMAGEABLE_H

#include <pxr/base/tf/token.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usd/common.h>

namespace godot_usd::translate {

enum ImageableConvertWarning {
	IMAGEABLE_CONVERT_WARN_PURPOSE_INVALID = 1 << 0,
	IMAGEABLE_CONVERT_WARN_VISIBILITY_INVALID = 1 << 1,
};

struct ImageableData {
	PXR_NS::UsdGeomImageable::PurposeInfo purpose_info;
	// The node's own visibility flag, not its effective visibility in the tree. Godot's Node3D.visible hides all
	// descendants of an invisible parent, the same way that visibility is inherited in USD.
	bool visible = true;
	uint32_t warn_flags = 0;
};

inline const PXR_NS::TfToken::Set default_hide_purposes = {PXR_NS::UsdGeomTokens->guide};

// During a recursive traversal, each prim's result.purpose_info must be supplied as p_parent_purpose_info for its
// children, seeded at the root with a default-constructed PurposeInfo.
//
// Hidden purposes are a parameter; valid purposes are those in UsdGeomImageable::GetOrderedPurposeTokens().
// A prim with any purpose not in p_hide_purposes is not hidden by purpose, and warns if its purpose is not in the valid
// set and this is the prim that authors it. Only gprims are hidden by purpose.
ImageableData to_imageable_data(const PXR_NS::UsdPrim &p_prim,
		const PXR_NS::UsdGeomImageable::PurposeInfo &p_parent_purpose_info,
		const PXR_NS::TfToken::Set &p_hide_purposes = default_hide_purposes);

} // namespace godot_usd::translate

#endif // GODOT_USD_BRIDGE_TRANSLATE_IMAGEABLE_H
