extends SceneTree

## Imports a USD stage and prints the resulting Godot node tree.
##
## A debugging/inspection tool, not a test framework — it reports what the
## importer produced and never asserts. CI greps its output; GUT (task 1.8)
## does the real assertions.
##
##   godot --headless --path demo -s tools/dump_stage.gd -- <path-to-usd-file>
##
## The path is handed straight to UsdBridge.import_stage(), which does its own
## file I/O, so it must be an OS path (absolute is safest) — not res://.
##
## Exit codes: 0 = imported, 1 = usage error or import failure.

## Surfaces with more vertices than this print each array's length instead of
## its contents, so a real asset's dump stays readable. Every fixture is well
## under it.
const MAX_PRINTED_VERTS := 64

func _init() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() != 1:
		printerr("usage: godot --headless --path demo -s tools/dump_stage.gd -- <path-to-usd-file>")
		quit(1)
		return

	var path := args[0]
	var bridge := UsdBridge.new()
	var root: Node3D = bridge.import_stage(path)
	if root == null:
		printerr("IMPORT FAILED: ", path)
		quit(1)
		return

	print("stage: ", path)
	var count := _dump(root, 0)
	print("nodes: ", count)

	# Nothing parented this tree, so we own it (see import_stage's contract).
	root.free()
	quit(0)

## Prints one line per node, depth-first; returns the number of nodes visited.
## Field order is stable so CI can match on whole substrings.
func _dump(node: Node, depth: int) -> int:
	var line := "  ".repeat(depth) + "- " + str(node.name)
	line += "  path=" + _meta_or(node, "usd_prim_path", "<none>")
	line += " type=" + _meta_or(node, "usd_type_name", "<none>")

	if node is Node3D:
		var n3 := node as Node3D
		var t := n3.transform
		line += " origin=" + _v3(t.origin)
		line += " scale=" + _v3(t.basis.get_scale())
		line += " euler_deg=" + _v3(t.basis.get_euler() * 180.0 / PI)
		if n3.is_set_as_top_level():
			line += " top_level=true"
		# The node's own flag. Nothing parents this tree, and outside the scene
		# tree is_visible_in_tree() ignores ancestors, so it could only repeat
		# this value.
		if not n3.visible:
			line += " visible=false"
	else:
		line += " class=" + node.get_class()

	# Always printed, and last before the mesh summary, so a needle that ends
	# in purpose= also pins the absence of the conditional fields before it.
	line += " purpose=" + _meta_or(node, "usd_purpose", "<none>")

	if node is MeshInstance3D:
		line += _mesh_summary((node as MeshInstance3D).mesh)

	print(line)

	var count := 1
	for child in node.get_children():
		count += _dump(child, depth + 1)
	return count

func _meta_or(node: Node, key: String, fallback: String) -> String:
	return str(node.get_meta(key)) if node.has_meta(key) else fallback

## Summarizes a MeshInstance3D's mesh: counts, then for each surface its index
## array, normals and UVs, then the AABB. A channel the surface lacks prints as
## <none>, and a surface over MAX_PRINTED_VERTS prints array lengths instead of
## contents.
##
## Normals print at 2 decimals because Godot stores them compressed: on 4.6.3 a
## normal read back through surface_get_arrays() can differ from the one written
## by about 1e-4 per component, and an axis-aligned one can come back with a
## tiny negative component. Positions and UVs come back exactly, so UVs keep 4
## decimals. In both, a value that rounds to zero prints without a sign.
func _mesh_summary(mesh: Mesh) -> String:
	if mesh == null:
		return "  mesh=<none>"
	var out := "  mesh='" + str(mesh.resource_name) + "'"
	out += " surfaces=" + str(mesh.get_surface_count())
	for s in mesh.get_surface_count():
		var arrays: Array = mesh.surface_get_arrays(s)
		var verts: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		var idx: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
		var print_arrays := verts.size() <= MAX_PRINTED_VERTS
		out += " surf%d[verts=%d indices=%d tris=%d]" % [s, verts.size(), idx.size(), idx.size() / 3]
		out += " idx=" + (str(Array(idx)) if print_arrays else _length(idx.size()))
		out += " nrm=" + _channel(arrays[Mesh.ARRAY_NORMAL], print_arrays, _normal)
		out += " uv=" + _channel(arrays[Mesh.ARRAY_TEX_UV], print_arrays, _uv)
	var aabb := mesh.get_aabb()
	out += " aabb_pos=" + _v3(aabb.position) + " aabb_size=" + _v3(aabb.size)
	return out

## Formats one channel array: <none> when the surface lacks it, its length when
## the surface is over the cap, and otherwise every value through format.
func _channel(values, print_arrays: bool, format: Callable) -> String:
	if values == null:
		return "<none>"
	if not print_arrays:
		return _length(values.size())
	var parts := PackedStringArray()
	for v in values:
		parts.append(format.call(v))
	return "[" + ", ".join(parts) + "]"

func _length(n: int) -> String:
	return "<%d values>" % n

func _normal(v: Vector3) -> String:
	return "(%s, %s, %s)" % [_num(v.x, 2), _num(v.y, 2), _num(v.z, 2)]

func _uv(v: Vector2) -> String:
	return "(%s, %s)" % [_num(v.x, 4), _num(v.y, 4)]

## Rounds to the given number of decimals, printing a value that rounds to zero
## without a sign, so -0.000015 and 0.0 both print as zero.
func _num(x: float, decimals: int) -> String:
	var s := ("%." + str(decimals) + "f") % x
	return s.trim_prefix("-") if float(s) == 0.0 else s

## Rounds to 4 decimals so float noise doesn't churn CI needles.
func _v3(v: Vector3) -> String:
	return "(%.4f, %.4f, %.4f)" % [v.x, v.y, v.z]
