#include <doctest/doctest.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec4d.h>
#include <pxr/base/gf/rotation.h>

#include <translate/xform_convert.h>

static bool is_affine(const PXR_NS::GfMatrix4d &p_usd_matrix){
	return p_usd_matrix.GetRow(3)[3] == 1 && p_usd_matrix.GetColumn(3) == PXR_NS::GfVec4d(0, 0, 0, 1);
}

TEST_CASE("xform_convert: identity GfMatrix4d to identity Transform3D"){
	auto usd_mat = PXR_NS::GfMatrix4d().SetIdentity();
	CHECK(is_affine(usd_mat));
	auto godot_tf = godot_usd::translate::to_godot_transform(usd_mat);
	CHECK(godot_tf.is_equal_approx(godot::Transform3D()));
}

TEST_CASE("xform_convert: USD translation matrix to Godot Transform3D"){
	auto usd_mat = PXR_NS::GfMatrix4d().SetTranslate(PXR_NS::GfVec3d(1, 2, 3));
	CHECK(is_affine(usd_mat));
	auto godot_tf = godot_usd::translate::to_godot_transform(usd_mat);
	auto expected = godot::Transform3D(godot::Basis(), godot::Vector3(1, 2, 3));
	CHECK(godot_tf.is_equal_approx(expected));
}

TEST_CASE("xform_convert: USD 90 degree rotation matrix to Godot Transform3D"){
	auto usd_mat = PXR_NS::GfMatrix4d().SetRotate(PXR_NS::GfRotation(PXR_NS::GfVec3d(1, 0, 0), 90));
	CHECK(is_affine(usd_mat));
	auto godot_tf = godot_usd::translate::to_godot_transform(usd_mat);
	CHECK(godot_tf.basis.xform(godot::Vector3(0, 0, 1)).is_equal_approx(godot::Vector3(0, -1, 0)));
	CHECK(godot_tf.basis.xform(godot::Vector3(0, 1, 0)).is_equal_approx(godot::Vector3(0, 0, 1)));
	CHECK(godot_tf.basis.xform(godot::Vector3(1, 0, 0)).is_equal_approx(godot::Vector3(1, 0, 0)));
}

TEST_CASE("xform_convert: USD non-uniform scale matrix to Godot Transform3D"){
	auto usd_mat = PXR_NS::GfMatrix4d().SetScale(PXR_NS::GfVec3d(1, 2, 3));
	CHECK(is_affine(usd_mat));
	auto godot_tf = godot_usd::translate::to_godot_transform(usd_mat);
	auto expected = godot::Transform3D(godot::Basis::from_scale(godot::Vector3(1, 2, 3)));
	CHECK(godot_tf.is_equal_approx(expected));
}

TEST_CASE("xform_convert: USD combined TRS matrix to Godot Transform3D"){
	// USD is row-vector, Godot column-vector, so composition order reverses
	// the conversion transposes, and (S * R)^T = R^T * S^T turns USD's S * R into Godot's R * S
	// (.rotated() pre-multiplies, so from_scale().rotated() => R * S)
	// T is last in the USD chain because Transform3D always translates last
	auto usd_mat_s = PXR_NS::GfMatrix4d().SetScale(PXR_NS::GfVec3d(1, 2, 3));
	auto usd_mat_r = PXR_NS::GfMatrix4d().SetRotate(PXR_NS::GfRotation(PXR_NS::GfVec3d(0, 1, 0), 90));
	auto usd_mat_t = PXR_NS::GfMatrix4d().SetTranslate(PXR_NS::GfVec3d(4, 5, 6));
	auto usd_mat = usd_mat_s * usd_mat_r * usd_mat_t;
	CHECK(is_affine(usd_mat));
	auto godot_tf = godot_usd::translate::to_godot_transform(usd_mat);
	auto basis = godot::Basis::from_scale(godot::Vector3(1, 2, 3)).rotated(godot::Vector3(0, 1, 0), Math_PI / 2);
	auto expected = godot::Transform3D(basis, godot::Vector3(4, 5, 6));
	CHECK(godot_tf.is_equal_approx(expected));
}
