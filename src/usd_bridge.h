#ifndef GODOT_USD_BRIDGE_USD_BRIDGE_H
#define GODOT_USD_BRIDGE_USD_BRIDGE_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/classes/node3d.hpp>

namespace godot_usd {

// INTERIM development surface — NOT the public API.
//
// Started as the M0 walking skeleton (prove the extension builds, links against
// godot-cpp, registers a class, and loads in the editor) and has since grown
// methods that drive the translation core from GDScript so each v0.1 task can
// be exercised in-engine before the real API exists. It is how the demo project
// and CI reach the importer today; treat every method as unstable.
//
// The v0.1 public API is UsdStageLoader.load(path) -> Node3D (spec section 4).
// When that lands, these methods should collapse into it or be removed —
// nothing outside demo/ and .github/workflows/ should depend on this class.
//
//   ping()         M0 liveness check.
//   open_stage()   Opens a stage and prints its prim tree (task 0.6).
//   import_stage() Builds a Node3D tree from the stage (task 1.2). The caller
//                  owns the returned root: parent it, or free it.
class UsdBridge : public godot::RefCounted {
	GDCLASS(UsdBridge, godot::RefCounted)

protected:
	static void _bind_methods();
	bool _are_plugins_registered() const;

public:
	godot::String ping() const;
	bool open_stage(const godot::String &p_path);
	godot::Node3D *import_stage(const godot::String &p_path);
};

} // namespace godot_usd

#endif // GODOT_USD_BRIDGE_USD_BRIDGE_H
