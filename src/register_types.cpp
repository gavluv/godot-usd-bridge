#include "register_types.h"

#include <gdextension_interface.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/core/error_macros.hpp>

#include <pxr/base/plug/registry.h>

#include "usd_bridge.h"

using namespace godot;

void initialize_godot_usd_module(ModuleInitializationLevel p_level) {
	// Must register plugins via PlugRegistry before any USD calls. SCENE initialization timing ensures that plugins are registered before any scripts are run.
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	String log_prefix = "[USD_BRIDGE] ";

	UtilityFunctions::print_verbose(log_prefix + "Initializing godot-usd-bridge module");

	PXR_NS::PlugRegistry& plug_registry = PXR_NS::PlugRegistry::GetInstance();

	if (plug_registry.GetAllPlugins().empty()) {
		String library_path;
		gdextension_interface::get_library_path(gdextension_interface::library, library_path._native_ptr());
		UtilityFunctions::print_verbose(log_prefix + "Library path: " + library_path);

		String resources_path = library_path.get_base_dir().get_base_dir().path_join("usd");
		UtilityFunctions::print_verbose(log_prefix + "USD resources path: " + resources_path);
		
		PXR_NS::PlugPluginPtrVector register_plugins_result = plug_registry.RegisterPlugins(resources_path.utf8().get_data());

		if (register_plugins_result.empty()) {
			ERR_PRINT(log_prefix + "Failed to register plugins at " + resources_path);
		} else {
			UtilityFunctions::print(log_prefix + "Registered " + String::num_uint64(register_plugins_result.size()) + " USD plugins");
		}
	} else {
		// Assume any plugins present in the registry are ours; breaks if a user sets PXR_PLUGINPATH_NAME globally.
		// Otherwise, this block is reached if the module is reloaded in the editor (currently disallowed).
		UtilityFunctions::print(log_prefix + "USD plugins already registered");
	}

	GDREGISTER_CLASS(godot_usd::UsdBridge);
}

void uninitialize_godot_usd_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

extern "C" {
// GDExtension entry point. The symbol name here must match `entry_symbol` in
// addons/godot-usd-bridge/godot-usd-bridge.gdextension.
GDExtensionBool GDE_EXPORT godot_usd_library_init(
		GDExtensionInterfaceGetProcAddress p_get_proc_address,
		GDExtensionClassLibraryPtr p_library,
		GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_godot_usd_module);
	init_obj.register_terminator(uninitialize_godot_usd_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
