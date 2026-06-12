import os
import sys

import bpy

repo = r"E:\SpaghettiKart"
fast64 = os.path.join(repo, "tools", "blender", "fast64")
scene_path = os.path.join(repo, "artifacts", "first-person-kart-screens", "meshy_kart_retopo_source.blend")
mesh_dir = r"E:\MarioArt\Meshes"
parts = [
    ("seat_candidate_low_poly", os.path.join(mesh_dir, "Meshy_AI_Mario_kart_64_go_kart_0609132241_texture.glb")),
    ("steering_wheel_candidate", os.path.join(mesh_dir, "Meshy_AI_Mario_kart_64_go_kart_0609133943_texture.glb")),
]

sys.path.insert(0, os.path.dirname(fast64))
try:
    import fast64

    fast64.register()
except Exception:
    pass

bpy.ops.wm.open_mainfile(filepath=scene_path)

for object_name, filepath in parts:
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.gltf(filepath=filepath)
    imported = [obj for obj in bpy.context.scene.objects if obj not in before]

    for obj in imported:
        obj.select_set(False)

    meshes = [obj for obj in imported if obj.type == "MESH"]
    for index, obj in enumerate(meshes):
        obj.name = object_name if len(meshes) == 1 else f"{object_name}_{index}"
        obj.data.name = f"{obj.name}_mesh"
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        obj.rotation_euler = (0.0, 0.0, 1.57079632679)
        obj.location.z -= 1.1
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        obj.select_set(False)
        print(
            f"{obj.name}: verts={len(obj.data.vertices)} polys={len(obj.data.polygons)} "
            f"dims=({obj.dimensions.x:.3f}, {obj.dimensions.y:.3f}, {obj.dimensions.z:.3f})"
        )

bpy.ops.wm.save_as_mainfile(filepath=scene_path)
print(f"Saved {scene_path}")
