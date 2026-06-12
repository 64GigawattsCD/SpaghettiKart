import sys

import bpy

scene_path = r"E:\SpaghettiKart\artifacts\first-person-kart-screens\meshy_kart_retopo_source.blend"
extension_root = r"C:\Users\Craig\AppData\Roaming\Blender Foundation\Blender\5.1\extensions\user_default"
port = 9878

if bpy.data.filepath != scene_path:
    bpy.ops.wm.open_mainfile(filepath=scene_path)

if extension_root not in sys.path:
    sys.path.insert(0, extension_root)

from mcp import mcp_to_blender_server as server

if not server.is_running():
    server.start("localhost", port)
    bpy.app.timers.register(server.poll, persistent=True)

print(f"SpaghettiKart retopo Blender MCP listening on 127.0.0.1:{port}")
