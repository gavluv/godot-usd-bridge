#ifndef GODOT_USD_BRIDGE_USD_BRIDGE_H
#define GODOT_USD_BRIDGE_USD_BRIDGE_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot_usd {

// M0 walking-skeleton surface: a trivial RefCounted exposed to GDScript so we
// can prove the extension builds, links against godot-cpp, registers a class,
// and loads in the editor. Real API (UsdStageLoader) arrives in v0.1.
class UsdBridge : public godot::RefCounted {
	GDCLASS(UsdBridge, godot::RefCounted)

protected:
	static void _bind_methods();
	bool _are_plugins_registered() const;

public:
	godot::String ping() const;
	bool open_stage(const godot::String &p_path);
};

} // namespace godot_usd

#endif // GODOT_USD_BRIDGE_USD_BRIDGE_H
