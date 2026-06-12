import os
import sys

import bpy

repo = r"E:\SpaghettiKart"
fast64 = os.path.join(repo, "tools", "blender", "fast64")
glb = r"E:\MarioArt\Meshes\Meshy_AI_mario_kart_64_empty_k_0608203211_texture.glb"
out = os.path.join(repo, "artifacts", "first-person-kart-screens", "blender_inspect.txt")

sys.path.insert(0, os.path.dirname(fast64))

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete()

fast64_status = "not attempted"
try:
    import fast64

    fast64.register()
    fast64_status = "registered"
except Exception as exc:
    fast64_status = f"failed: {type(exc).__name__}: {exc}"

bpy.ops.import_scene.gltf(filepath=glb)

lines = [f"Fast64: {fast64_status}"]
for obj in bpy.context.scene.objects:
    lines.append(f"Object: {obj.name} type={obj.type}")
    if obj.type == "MESH":
        mesh = obj.data
        lines.append(
            f"  verts={len(mesh.vertices)} edges={len(mesh.edges)} polys={len(mesh.polygons)} materials={len(mesh.materials)}"
        )
        lines.append(f"  dimensions={tuple(round(v, 4) for v in obj.dimensions)}")
        lines.append(f"  location={tuple(round(v, 4) for v in obj.location)}")
        lines.append(f"  rotation={tuple(round(v, 4) for v in obj.rotation_euler)}")
        for mat in mesh.materials:
            lines.append(f"  material={mat.name} use_nodes={mat.use_nodes}")

os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))

print("\n".join(lines))
