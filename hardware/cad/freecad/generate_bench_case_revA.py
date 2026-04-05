import os
import json

import FreeCAD as App
import Part
import Mesh
from FreeCAD import Vector


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
FREECAD_DIR = os.path.join(ROOT, "freecad")
RENDER_DIR = os.path.join(ROOT, "renders")
os.makedirs(FREECAD_DIR, exist_ok=True)
os.makedirs(RENDER_DIR, exist_ok=True)

PARAMS = {
    "project": "YouSimuladorOBD",
    "name": "bench_case_revA_base",
    "board_x": 120.0,
    "board_y": 85.0,
    "inner_clearance": 3.0,
    "floor_thickness": 3.0,
    "wall_thickness": 3.0,
    "wall_height": 22.0,
    "standoff_diameter": 10.0,
    "standoff_hole_diameter": 3.2,
    "standoff_height": 8.0,
    "mount_inset": 8.0,
    "obd_window_w": 34.0,
    "obd_window_h": 16.0,
    "obd_window_z": 6.0,
    "usb_window_w": 14.0,
    "usb_window_h": 8.0,
    "usb_window_z": 10.0,
}

inner_x = PARAMS["board_x"] + PARAMS["inner_clearance"] * 2.0
inner_y = PARAMS["board_y"] + PARAMS["inner_clearance"] * 2.0
outer_x = inner_x + PARAMS["wall_thickness"] * 2.0
outer_y = inner_y + PARAMS["wall_thickness"] * 2.0
outer_z = PARAMS["floor_thickness"] + PARAMS["wall_height"]

doc = App.newDocument("bench_case_revA_base")


def box(x, y, z, px=0.0, py=0.0, pz=0.0):
    return Part.makeBox(x, y, z, Vector(px, py, pz))


tray = box(outer_x, outer_y, outer_z)
inner_void = box(
    inner_x,
    inner_y,
    PARAMS["wall_height"] + 1.0,
    PARAMS["wall_thickness"],
    PARAMS["wall_thickness"],
    PARAMS["floor_thickness"],
)
tray = tray.cut(inner_void)

# Janela frontal para chicote OBD / passagem principal.
obd_cut = box(
    PARAMS["obd_window_w"],
    PARAMS["wall_thickness"] + 2.0,
    PARAMS["obd_window_h"],
    (outer_x - PARAMS["obd_window_w"]) / 2.0,
    -1.0,
    PARAMS["floor_thickness"] + PARAMS["obd_window_z"],
)
tray = tray.cut(obd_cut)

# Janela lateral para USB/programacao.
usb_cut = box(
    PARAMS["wall_thickness"] + 2.0,
    PARAMS["usb_window_w"],
    PARAMS["usb_window_h"],
    outer_x - PARAMS["wall_thickness"] - 1.0,
    (outer_y - PARAMS["usb_window_w"]) / 2.0,
    PARAMS["floor_thickness"] + PARAMS["usb_window_z"],
)
tray = tray.cut(usb_cut)

board_origin_x = PARAMS["wall_thickness"] + PARAMS["inner_clearance"]
board_origin_y = PARAMS["wall_thickness"] + PARAMS["inner_clearance"]

mount_positions = [
    (board_origin_x + PARAMS["mount_inset"], board_origin_y + PARAMS["mount_inset"]),
    (
        board_origin_x + PARAMS["board_x"] - PARAMS["mount_inset"],
        board_origin_y + PARAMS["mount_inset"],
    ),
    (
        board_origin_x + PARAMS["board_x"] - PARAMS["mount_inset"],
        board_origin_y + PARAMS["board_y"] - PARAMS["mount_inset"],
    ),
    (
        board_origin_x + PARAMS["mount_inset"],
        board_origin_y + PARAMS["board_y"] - PARAMS["mount_inset"],
    ),
]

for x, y in mount_positions:
    post = Part.makeCylinder(
        PARAMS["standoff_diameter"] / 2.0,
        PARAMS["standoff_height"],
        Vector(x, y, PARAMS["floor_thickness"]),
    )
    hole = Part.makeCylinder(
        PARAMS["standoff_hole_diameter"] / 2.0,
        PARAMS["standoff_height"] + PARAMS["floor_thickness"] + 1.0,
        Vector(x, y, -0.5),
    )
    tray = tray.fuse(post)
    tray = tray.cut(hole)

# Pes de apoio na base.
foot_d = 12.0
foot_h = 1.5
foot_offset = 12.0
foot_positions = [
    (foot_offset, foot_offset),
    (outer_x - foot_offset, foot_offset),
    (outer_x - foot_offset, outer_y - foot_offset),
    (foot_offset, outer_y - foot_offset),
]
for x, y in foot_positions:
    foot = Part.makeCylinder(foot_d / 2.0, foot_h, Vector(x, y, 0.0))
    tray = tray.fuse(foot)

obj = doc.addObject("Part::Feature", "bench_case_revA_base")
obj.Shape = tray
doc.recompute()

fcstd_path = os.path.join(FREECAD_DIR, "bench_case_revA_base.FCStd")
step_path = os.path.join(FREECAD_DIR, "bench_case_revA_base.step")
stl_path = os.path.join(FREECAD_DIR, "bench_case_revA_base.stl")
params_path = os.path.join(FREECAD_DIR, "bench_case_revA_base_params.json")

doc.saveAs(fcstd_path)
Part.export([obj], step_path)
Mesh.export([obj], stl_path)

with open(params_path, "w", encoding="utf-8") as f:
    json.dump(
        {
            "params": PARAMS,
            "derived": {
                "inner_x": inner_x,
                "inner_y": inner_y,
                "outer_x": outer_x,
                "outer_y": outer_y,
                "outer_z": outer_z,
            },
        },
        f,
        indent=2,
    )

print("Generated:")
print(fcstd_path)
print(step_path)
print(stl_path)
print(params_path)

