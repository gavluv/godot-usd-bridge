#include "usd_bridge.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/core/error_macros.hpp>

#include <pxr/base/plug/registry.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/primRange.h>

using namespace godot;

namespace godot_usd {

void UsdBridge::_bind_methods() {
	ClassDB::bind_method(D_METHOD("ping"), &UsdBridge::ping);
	ClassDB::bind_method(D_METHOD("open_stage", "path"), &UsdBridge::open_stage);
}

String UsdBridge::ping() const {
	return "godot-usd-bridge: pong";
}

bool UsdBridge::_are_plugins_registered() const {
	return PXR_NS::PlugRegistry::GetInstance().GetPluginWithName("usd") != nullptr;
}

bool UsdBridge::open_stage(const String &p_path) {
	UtilityFunctions::print("Opening USD stage: " + p_path);

	if (!_are_plugins_registered()) {
		ERR_PRINT("USD plugins are not registered; cannot open stage");
		return false;
	}

	auto stage = PXR_NS::UsdStage::Open(p_path.utf8().get_data());
	if (stage == nullptr) {
		ERR_PRINT("Failed to open USD stage: " + p_path);
		return false;
	}

	for (const auto &prim : stage->Traverse()) {
		UtilityFunctions::print(String(prim.GetPath().GetText()) + " (" + String(prim.GetTypeName().GetText()) + ")");
	}

	return true;
}

} // namespace godot_usd
