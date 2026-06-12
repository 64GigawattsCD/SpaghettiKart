import os
import sys

import bpy

repo = r"E:\SpaghettiKart"
fast64 = os.path.join(repo, "tools", "blender", "fast64")
glb = r"E:\MarioArt\Meshes\Meshy_AI_mario_kart_64_empty_k_0608203211_texture.glb"
out = os.path.join(repo, "artifacts", "first-person-kart-screens", "meshy_kart_retopo_source.blend")

sys.path.insert(0, os.path.dirname(fast64))
try:
    import fast64

    fast64.register()
except Exception:
    pass

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete()
bpy.ops.import_scene.gltf(filepath=glb)

obj = next(obj for obj in bpy.context.scene.objects if obj.type == "MESH")
obj.name = "retopo_source_full_density_game_fit"
obj.data.name = "retopo_source_full_density_game_fit_mesh"
bpy.context.view_layer.objects.active = obj
obj.select_set(True)

# Keep the full source mesh, but bake the orientation/origin that matched in game:
# original glTF: X=width, Y=depth, Z=height
# game fit: X=width, Y=forward, Z=up, with the tested 90-degree CCW yaw offset.
obj.rotation_euler = (0.0, 0.0, 1.57079632679)
obj.location = (0.0, 0.0, -1.1)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

empty = bpy.data.objects.new("game_pivot_player_pos", None)
empty.empty_display_type = "PLAIN_AXES"
empty.empty_display_size = 1.5
empty.location = (0.0, 0.0, 0.0)
bpy.context.collection.objects.link(empty)

for name, loc, scale in [
    ("game_width_x_reference", (0.0, 0.0, 0.02), (9.5, 0.04, 0.04)),
    ("game_forward_y_reference", (0.0, 0.0, 0.08), (0.04, 6.0, 0.04)),
    ("approx_eye_height_5_5", (0.0, 0.0, 5.5), (1.0, 0.12, 0.12)),
]:
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
    marker = bpy.context.object
    marker.name = name
    marker.scale = scale
    marker.display_type = "WIRE"

bpy.ops.object.light_add(type="AREA", location=(0.0, -6.0, 8.0))
bpy.context.object.name = "retopo_scene_area_light"
bpy.context.object.data.energy = 500
bpy.context.object.data.size = 5

bpy.ops.object.camera_add(location=(0.0, -13.0, 5.5), rotation=(1.22173, 0.0, 0.0))
bpy.context.scene.camera = bpy.context.object

bpy.ops.wm.save_as_mainfile(filepath=out)
print(out)
