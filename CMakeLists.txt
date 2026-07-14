cmake_minimum_required(VERSION 3.26)

project(godot-usd-bridge LANGUAGES CXX)

# ---------------------------------------------------------------------------
# godot-cpp (extern/godot-cpp submodule, pinned per ADR-004).
#
# Four load-bearing settings are required so this extension can later link
# against OpenUSD's monolithic `usd_ms.dll`. They are pinned in CMakePresets
# (NOT here, and never via ad-hoc command-line flags — see CLAUDE.md). The
# option names below were verified against the pinned godot-cpp commit:
#
#   /MD (dynamic CRT)   GODOTCPP_USE_STATIC_CPP=OFF     cmake/windows.cmake:73-91
#                       (default ON => /MT static CRT, which USD cannot match)
#   exceptions ON       GODOTCPP_DISABLE_EXCEPTIONS=OFF cmake/godotcpp.cmake:167
#                       (default ON sets _HAS_EXCEPTIONS=0 on MSVC)
#   RTTI ON             no override needed — godot-cpp emits no /GR-, and MSVC
#                       enables RTTI (/GR) by default
#   Godot 4.6 API       GODOTCPP_API_VERSION=4.6        cmake/godotcpp.cmake:267
#                       (4.6 is also GODOTCPP_LATEST_API_VERSION / the default)
# ---------------------------------------------------------------------------
add_subdirectory(extern/godot-cpp)

# ---------------------------------------------------------------------------
# OpenUSD (stage-1 output; scripts/build_usd.ps1 -> thirdparty/usd-install/).
# find_package locates pxrConfig.cmake through CMAKE_PREFIX_PATH, which the
# presets point at the install root. The monolithic build exposes one imported
# target, `usd_m` (usd_ms.lib / usd_ms.dll -- cmake/pxrTargets-release.cmake),
# and pxrConfig chains find_dependency(TBB) / find_dependency(MaterialX) from
# config packages under <install>/lib/cmake/.
# ---------------------------------------------------------------------------
find_package(pxr CONFIG REQUIRED)

# Stage 1 only builds a Release USD; our Debug preset links it too. This is
# deliberate and safe: both configs compile with the release dynamic CRT (/MD,
# never /MDd -- see GODOTCPP_DEBUG_CRT default), so no CRT mismatch results.
set_target_properties(usd_m PROPERTIES MAP_IMPORTED_CONFIG_DEBUG "Release")

add_library(godot-usd SHARED)

# The hand-written translation core (src/translate/). Shared between the
# extension and the doctest unit-test executable, so every translate/ file
# added here is compiled into both. Populated starting with task 1.1.
set(TRANSLATE_SOURCES)

target_sources(godot-usd PRIVATE
	src/register_types.cpp
	src/usd_bridge.cpp
	${TRANSLATE_SOURCES}
)

target_link_libraries(godot-usd PRIVATE godot::cpp usd_m)

# PXR_INCLUDE_DIRS comes from pxrConfig.cmake; NOMINMAX because USD headers
# include windows.h, whose min/max macros break std::min/std::max.
target_include_directories(godot-usd PRIVATE ${PXR_INCLUDE_DIRS})
target_compile_definitions(godot-usd PRIVATE NOMINMAX)

# Match godot-cpp's own target settings so the ABI lines up.
set_target_properties(godot-usd PROPERTIES
	CXX_STANDARD 17
	CXX_EXTENSIONS OFF
	POSITION_INDEPENDENT_CODE ON
)

# Derive the platform/target/arch suffix straight from godot-cpp so the produced
# file name matches the [libraries] entries in the .gdextension manifest, e.g.
# libgodot-usd.windows.template_debug.x86_64.dll  (mirrors godot-cpp/test).
get_target_property(GODOTCPP_SUFFIX godot-cpp GODOTCPP_SUFFIX)

set(ADDON_DIR "${CMAKE_SOURCE_DIR}/addons/godot-usd-bridge")

set_target_properties(godot-usd PROPERTIES
	PREFIX "lib"
	OUTPUT_NAME "godot-usd${GODOTCPP_SUFFIX}"
	# Wrapped in $<1:...> so the MSVC generator doesn't append a per-config dir.
	RUNTIME_OUTPUT_DIRECTORY "$<1:${ADDON_DIR}/bin>"
	LIBRARY_OUTPUT_DIRECTORY "$<1:${ADDON_DIR}/bin>"
	PDB_OUTPUT_DIRECTORY     "$<1:${ADDON_DIR}/bin>"
)

# Deploy the runtime pieces into the addon dir (all deployed files gitignored):
#   bin/ <- usd_ms.dll + its verified load-time closure (tbb12, MaterialXCore,
#          MaterialXFormat -- from dumpbin /dependents; see docs/building.md).
#          The extension DLL already lands here via the output dirs above.
#   usd/ <- the plugInfo.json resource tree from <install>/lib/usd. USD cannot
#          function without it (metadata is read from disk at runtime, not
#          compiled into usd_ms.dll). A .gdignore keeps the Godot editor from
#          importing the tree's thousands of files as project assets.
# Then mirror the whole addon into demo/addons/ (gitignored) so `godot --path
# demo` can load it; the canonical addon stays at the repo-root addons/ dir.
set(USD_INSTALL_DIR "${CMAKE_SOURCE_DIR}/thirdparty/usd-install")
set(DEMO_ADDON_DIR "${CMAKE_SOURCE_DIR}/demo/addons/godot-usd-bridge")
add_custom_command(TARGET godot-usd POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${USD_INSTALL_DIR}/lib/usd_ms.dll"
		"${USD_INSTALL_DIR}/bin/tbb12.dll"
		"${USD_INSTALL_DIR}/bin/MaterialXCore.dll"
		"${USD_INSTALL_DIR}/bin/MaterialXFormat.dll"
		"${ADDON_DIR}/bin"
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
		"${USD_INSTALL_DIR}/lib/usd"
		"${ADDON_DIR}/usd"
	COMMAND ${CMAKE_COMMAND} -E touch "${ADDON_DIR}/usd/.gdignore"
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
		"${ADDON_DIR}"
		"${DEMO_ADDON_DIR}"
	VERBATIM
)

# ---------------------------------------------------------------------------
# Unit tests (doctest -- extern/doctest submodule, pinned; see ADR-005).
# A plain console exe: translate/ is pure math over engine-independent
# godot-cpp types, so no running engine is involved. The custom main()
# (tests/main.cpp) registers USD's plugins before any test runs -- required
# for stage-reading tests; see docs/building.md.
# ---------------------------------------------------------------------------
option(GODOT_USD_BUILD_TESTS "Build the doctest unit-test executable" ON)

if(GODOT_USD_BUILD_TESTS)
	enable_testing()

	add_executable(godot-usd-tests
		tests/main.cpp
		tests/test_harness.cpp
		${TRANSLATE_SOURCES}
	)

	target_include_directories(godot-usd-tests PRIVATE
		"${CMAKE_SOURCE_DIR}/extern/doctest"
		"${CMAKE_SOURCE_DIR}/src"
		${PXR_INCLUDE_DIRS}
	)

	# godot::cpp for the (engine-independent) math types translate/ uses;
	# usd_m for the stage-reading halves and harness checks.
	target_link_libraries(godot-usd-tests PRIVATE godot::cpp usd_m)

	# Baked absolute paths: fine for a local/CI test binary, never shipped.
	target_compile_definitions(godot-usd-tests PRIVATE
		NOMINMAX
		GODOT_USD_TEST_PLUGIN_DIR="${USD_INSTALL_DIR}/lib/usd"
		GODOT_USD_TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
	)

	set_target_properties(godot-usd-tests PROPERTIES
		CXX_STANDARD 17
		CXX_EXTENSIONS OFF
	)

	# The exe loads usd_ms.dll at start; give it the 4-DLL closure beside it.
	add_custom_command(TARGET godot-usd-tests POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${USD_INSTALL_DIR}/lib/usd_ms.dll"
			"${USD_INSTALL_DIR}/bin/tbb12.dll"
			"${USD_INSTALL_DIR}/bin/MaterialXCore.dll"
			"${USD_INSTALL_DIR}/bin/MaterialXFormat.dll"
			"$<TARGET_FILE_DIR:godot-usd-tests>"
		VERBATIM
	)

	add_test(NAME godot-usd-unit COMMAND godot-usd-tests)
endif()
