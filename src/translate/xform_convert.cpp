#include "xform_convert.h"

#include <pxr/base/gf/vec4d.h>

namespace godot_usd::translate {

godot::Transform3D to_godot_transform(const PXR_NS::GfMatrix4d &p_usd_matrix){
	auto basis = godot::Basis();

	PXR_NS::GfVec4d usd_row;
	for(int i = 0; i < 3; ++i){
		usd_row = p_usd_matrix.GetRow(i);
		basis.set_column(i, godot::Vector3(usd_row[0], usd_row[1], usd_row[2]));
	}

	usd_row = p_usd_matrix.GetRow(3);
	auto origin = godot::Vector3(usd_row[0], usd_row[1], usd_row[2]);

	return godot::Transform3D(basis, origin);
}

} // namespace godot_usd::translate
