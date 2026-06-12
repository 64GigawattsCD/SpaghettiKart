import bpy

for obj in sorted(bpy.context.scene.objects, key=lambda o: o.name.lower()):
    if obj.type != "MESH":
        continue
    selected = "*" if obj.select_get() else " "
    dims = tuple(round(v, 3) for v in obj.dimensions)
    loc = tuple(round(v, 3) for v in obj.location)
    print(f"{selected} {obj.name} dims={dims} loc={loc} verts={len(obj.data.vertices)} polys={len(obj.data.polygons)}")
