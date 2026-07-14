#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <pxr/base/plug/registry.h>

#include <cstdio>
#include <string>

// Test-harness entry point. USD is bootstrapped here, before any test runs:
// stage-reading tests need the plugInfo resource tree registered, and USD
// fatally asserts (rather than failing politely) when its file-format plugins
// are absent -- the same hazard the extension's init guards against.
//
// GODOT_USD_TEST_PLUGIN_DIR is baked in by CMake and points at the stage-1
// install (thirdparty/usd-install/lib/usd), so tests exercise the exact USD
// build the extension links.
int main(int argc, char **argv) {
	PXR_NS::PlugRegistry &registry = PXR_NS::PlugRegistry::GetInstance();
	registry.RegisterPlugins(std::string(GODOT_USD_TEST_PLUGIN_DIR));
	if (registry.GetPluginWithName("usd") == nullptr) {
		std::fprintf(stderr,
				"FATAL: USD plugin registration failed (looked in: %s).\n"
				"Run scripts/build_usd.ps1 first -- see docs/building.md.\n",
				GODOT_USD_TEST_PLUGIN_DIR);
		return 1;
	}

	doctest::Context context;
	context.applyCommandLine(argc, argv);
	// No post-run application code, so no shouldExit() dance is needed:
	// run() also services query flags like --list-test-cases.
	return context.run();
}
