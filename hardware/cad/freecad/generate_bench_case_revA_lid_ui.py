import os
import json

import FreeCAD as App
import Part
import Mesh
from FreeCAD import Vector


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
FREECAD_DIR = os.path.join(ROOT, "freecad")
os.makedirs(FREECAD_DIR, exist_ok=True)

PARAMS = {
    "project": "YouSimuladorOBD",
    "name": "bench_case_revA_lid_ui",
    "base_outer_x": 132.0,
    "base_outer_y": 97.0,
    "wall_thickness": 3.0,
    "lid_thickness": 2.4,
    "lip_thickness": 1.6,
    "lip_depth": 6.0,
    "fit_clearance": 0.35,
    # O firmware usa SH1107 128x128, entao a janela visivel precisa ser quadrada.
    "oled_window_x": 34.0,
    "oled_window_y": 34.0,
    "oled_window_radius": 2.0,
    "oled_window_offset_y": 14.0,
    "oled_bezel_x": 42.0,
    "oled_bezel_y": 42.0,
    "oled_bezel_depth": 0.8,
    "button_diameter": 6.5,
    "button_pitch": 14.0,
    "button_row_y": 26.0,
    "button_names": ["PREV", "NEXT", "UP", "DOWN", "OK", "PROTO"],
    "encoder_hole_diameter": 7.4,
    "encoder_clear_diameter": 17.0,
    "encoder_center_x": 103.0,
    "encoder_center_y": 53.0,
}

outer_x = PARAMS["base_outer_x"]
outer_y = PARAMS["base_outer_y"]
plate_z = PARAMS["lid_thickness"]

inner_lip_x = outer_x - 2.0 * (PARAMS["wall_thickness"] + PARAMS["fit_clearance"])
inner_lip_y = outer_y - 2.0 * (PARAMS["wall_thickness"] + PARAMS["fit_clearance"])

doc = App.newDocument("bench_case_revA_lid_ui")


def box(x, y, z, px=0.0, py=0.0, pz=0.0):
    return Part.makeBox(x, y, z, Vector(px, py, pz))


def cylinder(d, h, px, py, pz=0.0):
    return Part.makeCylinder(d / 2.0, h, Vector(px, py, pz))


lid = box(outer_x, outer_y, plate_z)

lip_outer = box(
    inner_lip_x,
    inner_lip_y,
    PARAMS["lip_depth"],
    (outer_x - inner_lip_x) / 2.0,
    (outer_y - inner_lip_y) / 2.0,
    -PARAMS["lip_depth"],
)
lip_inner = box(
    inner_lip_x - 2.0 * PARAMS["lip_thickness"],
    inner_lip_y - 2.0 * PARAMS["lip_thickness"],
    PARAMS["lip_depth"] + 1.0,
    (outer_x - inner_lip_x) / 2.0 + PARAMS["lip_thickness"],
    (outer_y - inner_lip_y) / 2.0 + PARAMS["lip_thickness"],
    -PARAMS["lip_depth"] - 0.5,
)

lid = lid.fuse(lip_outer.cut(lip_inner))

# Janela do OLED.
oled_px = (outer_x - PARAMS["oled_window_x"]) / 2.0
oled_py = outer_y - PARAMS["oled_window_offset_y"] - PARAMS["oled_window_y"]
oled_cut = box(
    PARAMS["oled_window_x"],
    PARAMS["oled_window_y"],
    plate_z + 1.0,
    oled_px,
    oled_py,
    -0.5,
)
lid = lid.cut(oled_cut)

# Rebaixo raso ao redor da janela para aproximar o painel do modulo real.
oled_bezel_px = (outer_x - PARAMS["oled_bezel_x"]) / 2.0
oled_bezel_py = outer_y - PARAMS["oled_window_offset_y"] - PARAMS["oled_bezel_y"]
oled_bezel = box(
    PARAMS["oled_bezel_x"],
    PARAMS["oled_bezel_y"],
    PARAMS["oled_bezel_depth"],
    oled_bezel_px,
    oled_bezel_py,
    plate_z - PARAMS["oled_bezel_depth"],
)
lid = lid.cut(oled_bezel)

# Layout real do projeto: seis botoes dedicados abaixo do display.
center_x = outer_x / 2.0
button_row_y = PARAMS["button_row_y"]
button_pitch = PARAMS["button_pitch"]
button_positions = []

button_count = len(PARAMS["button_names"])
first_button_x = center_x - ((button_count - 1) * button_pitch) / 2.0
for idx in range(button_count):
    x = first_button_x + idx * button_pitch
    y = button_row_y
    button_positions.append((x, y))
    lid = lid.cut(cylinder(PARAMS["button_diameter"], plate_z + 1.0, x, y, -0.5))

# Encoder separado, como na UI fisica do firmware.
encoder_x = PARAMS["encoder_center_x"]
encoder_y = PARAMS["encoder_center_y"]
lid = lid.cut(
    cylinder(
        PARAMS["encoder_hole_diameter"],
        plate_z + 1.0,
        encoder_x,
        encoder_y,
        -0.5,
    )
)

# Rebaixo raso para area de manipulacao do knob.
encoder_clear = cylinder(
    PARAMS["encoder_clear_diameter"],
    min(1.0, plate_z),
    encoder_x,
    encoder_y,
    plate_z - min(1.0, plate_z),
)
lid = lid.cut(encoder_clear)

obj = doc.addObject("Part::Feature", "bench_case_revA_lid_ui")
obj.Shape = lid
doc.recompute()

fcstd_path = os.path.join(FREECAD_DIR, "bench_case_revA_lid_ui.FCStd")
step_path = os.path.join(FREECAD_DIR, "bench_case_revA_lid_ui.step")
stl_path = os.path.join(FREECAD_DIR, "bench_case_revA_lid_ui.stl")
params_path = os.path.join(FREECAD_DIR, "bench_case_revA_lid_ui_params.json")

doc.saveAs(fcstd_path)
Part.export([obj], step_path)
Mesh.export([obj], stl_path)

with open(params_path, "w", encoding="utf-8") as f:
    json.dump(
        {
            "params": PARAMS,
            "derived": {
                "inner_lip_x": inner_lip_x,
                "inner_lip_y": inner_lip_y,
                "oled_window_position": [oled_px, oled_py],
                "oled_bezel_position": [oled_bezel_px, oled_bezel_py],
                "button_positions": button_positions,
                "encoder_position": [encoder_x, encoder_y],
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
