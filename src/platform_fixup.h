#ifndef GODOT_USD_BRIDGE_PLATFORM_FIXUP_H
#define GODOT_USD_BRIDGE_PLATFORM_FIXUP_H

// Force-included into every translation unit by CMake (/FI on MSVC); it is
// never #included by hand.
//
// OpenUSD's headers pull in <windows.h>, which defines macros that collide
// with godot-cpp identifiers. The one that bites is CONNECT_DEFERRED from
// winnetwk.h, which mangles the ConnectFlags enum in
// godot_cpp/classes/object.hpp and produces ~200 syntax errors inside a
// godot-cpp header, naming neither USD nor the file being compiled.
//
// Loading <windows.h> here and undefining the offenders once fixes it for the
// whole translation unit: <windows.h> has its own include guard, so USD's
// later include is a no-op and cannot reintroduce the macros. Include order
// then stops mattering anywhere in the project.
//
// NOMINMAX and WIN32_LEAN_AND_MEAN are set by CMake. NOMINMAX suppresses the
// min/max macros; WIN32_LEAN_AND_MEAN does NOT cover winnetwk.h, which
// <windows.h> includes above that guard, hence the explicit #undef below.
//
// Add to the list as new collisions surface.

#ifdef _WIN32

#include <windows.h>

// winnetwk.h — collides with godot::Object::ConnectFlags::CONNECT_DEFERRED.
#undef CONNECT_DEFERRED

#endif // _WIN32

#endif // GODOT_USD_BRIDGE_PLATFORM_FIXUP_H
