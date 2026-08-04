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
	else:
		line += " class=" + node.get_class()

	if node is MeshInstance3D:
		line += _mesh_summary((node as MeshInstance3D).mesh)

	print(line)

	var count := 1
	for child in node.get_children():
		count += _dump(child, depth + 1)
	return count

func _meta_or(node: Node, key: String, fallback: String) -> String:
	return str(node.get_meta(key)) if node.has_meta(key) else fallback

## Summarizes a MeshInstance3D's mesh structurally. Task 1.3 produces no
## normals (1.4) and unflipped winding (1.5), so meshes are not meaningfully
## viewable — counts and AABB are how you verify them.
func _mesh_summary(mesh: Mesh) -> String:
	if mesh == null:
		return "  mesh=<none>"
	var out := "  mesh='" + str(mesh.resource_name) + "'"
	out += " surfaces=" + str(mesh.get_surface_count())
	for s in mesh.get_surface_count():
		var arrays: Array = mesh.surface_get_arrays(s)
		var verts: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		var idx: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
		out += " surf%d[verts=%d indices=%d tris=%d]" % [s, verts.size(), idx.size(), idx.size() / 3]
		out += " idx=" + str(Array(idx))
	var aabb := mesh.get_aabb()
	out += " aabb_pos=" + _v3(aabb.position) + " aabb_size=" + _v3(aabb.size)
	return out

## Rounds to 4 decimals so float noise doesn't churn CI needles.
func _v3(v: Vector3) -> String:
	return "(%.4f, %.4f, %.4f)" % [v.x, v.y, v.z]
