#include "imageable.h"

#include <pxr/base/tf/token.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/gprim.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <algorithm>

namespace godot_usd::translate {

static bool is_valid_purpose(const PXR_NS::TfToken &p_purpose) {
	const auto &ordered_purposes = PXR_NS::UsdGeomImageable::GetOrderedPurposeTokens();
	return std::find(ordered_purposes.begin(), ordered_purposes.end(), p_purpose) != ordered_purposes.end();
}

ImageableData to_imageable_data(const PXR_NS::UsdPrim &p_prim,
		const PXR_NS::UsdGeomImageable::PurposeInfo &p_parent_purpose_info,
		const PXR_NS::TfToken::Set &p_hide_purposes) {
	ImageableData result;
	auto imageable = PXR_NS::UsdGeomImageable(p_prim);
	// Always compute purpose_info so that purpose flows through non-Imageable prims to their descendants.
	result.purpose_info = imageable.ComputePurposeInfo(p_parent_purpose_info);

	if (!imageable) {
		// Only Imageable prims are read, so an untyped prim's opinion is ignored as USD ignores it.
		// GetVisibilityAttr() reads the attribute by name on untyped prims; ComputeVisibility checks the schema's
		// validity before reading it (pxr/usd/usdGeom/imageable.cpp), as of OpenUSD v26.05.
		return result;
	}

	PXR_NS::TfToken visibility_token;
	// Fall back to visible silently if visibility is blocked (Get() returns false).
	if (imageable.GetVisibilityAttr().Get(&visibility_token)) {
		if (visibility_token == PXR_NS::UsdGeomTokens->invisible) {
			// Set visible = false for this prim only; Godot does the inheritance.
			result.visible = false;
		} else if (visibility_token != PXR_NS::UsdGeomTokens->inherited) {
			// Fall back to visible and warn if authored visibility is not "invisible" or "inherited".
			result.warn_flags |= IMAGEABLE_CONVERT_WARN_VISIBILITY_INVALID;
		}
	}

	if (p_hide_purposes.find(result.purpose_info.purpose) != p_hide_purposes.end()) {
		// Hide only if this is a gprim because purpose doesn't prune but Node3D.visible does.
		// Gprim over UsdGeomBoundable because SkelRoot is boundable and has children that hiding would prune.
		// Purpose only ever clears the visible flag; it never makes an invisible prim visible again.
		result.visible &= !p_prim.IsA<PXR_NS::UsdGeomGprim>();
	} else if (!is_valid_purpose(result.purpose_info.purpose) && imageable.GetPurposeAttr().HasAuthoredValue()) {
		// Fall back to visible and warn if purpose is not one of the valid tokens in
		// UsdGeomImageable::GetOrderedPurposeTokens().
		// Only warn on prims that author an invalid purpose, not on prims that inherit one from an ancestor.
		result.warn_flags |= IMAGEABLE_CONVERT_WARN_PURPOSE_INVALID;
	}

	return result;
}

} // namespace godot_usd::translate
