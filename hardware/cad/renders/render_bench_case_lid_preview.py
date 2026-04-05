import os
import sys
import math
import bpy
from mathutils import Vector


argv = sys.argv
if "--" in argv:
    argv = argv[argv.index("--") + 1 :]
else:
    argv = []

if len(argv) < 2:
    raise SystemExit("Usage: blender -b -P render_bench_case_lid_preview.py -- input.stl output.png")

input_stl = argv[0]
output_png = argv[1]

bpy.ops.wm.read_factory_settings(use_empty=True)

if hasattr(bpy.ops.wm, "stl_import"):
    bpy.ops.wm.stl_import(filepath=input_stl)
else:
    bpy.ops.import_mesh.stl(filepath=input_stl)

obj = bpy.context.selected_objects[0]
bpy.context.view_layer.objects.active = obj

mat = bpy.data.materials.new(name="LidMaterial")
mat.use_nodes = True
bsdf = mat.node_tree.nodes["Principled BSDF"]
bsdf.inputs["Base Color"].default_value = (0.06, 0.14, 0.25, 1.0)
bsdf.inputs["Roughness"].default_value = 0.48
obj.data.materials.append(mat)

bbox = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
min_x = min(v.x for v in bbox)
max_x = max(v.x for v in bbox)
min_y = min(v.y for v in bbox)
max_y = max(v.y for v in bbox)
min_z = min(v.z for v in bbox)
max_z = max(v.z for v in bbox)

center = Vector(((min_x + max_x) / 2.0, (min_y + max_y) / 2.0, (min_z + max_z) / 2.0))
size = max(max_x - min_x, max_y - min_y, max_z - min_z)

cam_data = bpy.data.cameras.new("Camera")
cam = bpy.data.objects.new("Camera", cam_data)
bpy.context.scene.collection.objects.link(cam)
cam.location = center + Vector((size * 0.85, -size * 1.05, size * 0.95))
cam.rotation_euler = (math.radians(58), 0.0, math.radians(34))
cam.data.type = "ORTHO"
cam.data.ortho_scale = size * 1.7
bpy.context.scene.camera = cam

sun_data = bpy.data.lights.new(name="Sun", type="SUN")
sun_data.energy = 3.5
sun = bpy.data.objects.new(name="Sun", object_data=sun_data)
bpy.context.scene.collection.objects.link(sun)
sun.rotation_euler = (math.radians(40), 0.0, math.radians(15))

world = bpy.data.worlds.get("World")
if world is None:
    world = bpy.data.worlds.new("World")
bpy.context.scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes["Background"]
bg.inputs[0].default_value = (0.985, 0.99, 1.0, 1.0)
bg.inputs[1].default_value = 0.9

scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE"
scene.render.image_settings.file_format = "PNG"
scene.render.resolution_x = 1600
scene.render.resolution_y = 1200
scene.render.filepath = output_png

os.makedirs(os.path.dirname(output_png), exist_ok=True)
bpy.ops.render.render(write_still=True)
