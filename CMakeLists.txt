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

add_library(godot-usd SHARED)

target_sources(godot-usd PRIVATE
	src/register_types.cpp
	src/usd_bridge.cpp
)

target_link_libraries(godot-usd PRIVATE godot::cpp)

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

# Make the demo project self-contained: copy the committed manifest and the
# freshly built DLL into demo/addons/godot-usd-bridge/ (gitignored) so that
# `godot --path demo` resolves the res://-relative library path. The canonical
# committed addon lives at the repo-root addons/ dir; this is just a local
# install of it for the demo.
set(DEMO_ADDON_DIR "${CMAKE_SOURCE_DIR}/demo/addons/godot-usd-bridge")
add_custom_command(TARGET godot-usd POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory "${DEMO_ADDON_DIR}/bin"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${ADDON_DIR}/godot-usd-bridge.gdextension"
		"${DEMO_ADDON_DIR}/godot-usd-bridge.gdextension"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE:godot-usd>"
		"${DEMO_ADDON_DIR}/bin/"
	VERBATIM
)
