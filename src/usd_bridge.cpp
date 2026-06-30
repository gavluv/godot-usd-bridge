#include "usd_bridge.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace godot_usd {

void UsdBridge::_bind_methods() {
	ClassDB::bind_method(D_METHOD("ping"), &UsdBridge::ping);
}

String UsdBridge::ping() const {
	return "godot-usd-bridge: pong";
}

} // namespace godot_usd
