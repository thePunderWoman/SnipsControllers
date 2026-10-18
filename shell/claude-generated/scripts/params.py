"""Snips controller shell: every tunable number lives here (mm).

Frame (board-local, right-hand build):
  X  across the board, 0..BOARD_W
  Y  along the board, 0 = palm/USB end, BOARD_L = top/screen end
  Z  0 = board BACK face, BOARD_T = board FRONT face (thumb side, +Z toward the user)
STEP -> local:  x - 120.0,  y + 159.9,  z unchanged.
"""
import math

# ---------- board (measured from PCB STEP) ----------
BOARD_W, BOARD_L, BOARD_T = 35.0, 99.9, 1.51
STEP_SHIFT = (-120.0, 159.9, 0.0)

# ---------- feature positions (local frame; from STEP + KiCad refs) ----------
STICK_XY = (17.5, 64.65)
STICK_BODY = 13.2          # square housing
STICK_BODY_TOP = 13.5      # housing top above back face (ALPS stand-in!)
STICK_CAP_TOP = 21.25      # cap top above back face (stand-in; verify with real GuliKit)
MACRO_X = (7.25, 17.5, 27.75)
MACRO_Y = (47.4, 38.65)
MACRO_TOP = 6.59
XBEE_BOX = (2.09, 9.21, 29.72, 33.59, 11.23)    # x0, y0, x1, y1, ztop
USB_XY = (17.5, 2.8)       # HRO TYPE-C-31-M-12, plug faces -Y
PWR_XY = (9.0, 1.95)       # side-actuated Alps, faces -Y
LED_XY = (26.55, 3.25)     # SK6812 mini, front face
SIDE_SW_Y = (69.4, 59.37)  # left edge x=1.75, right edge x=33.2
SIDE_SW_X = (1.75, 33.2)
BUMPER_XY = (4.72, 98.15)  # faces +Y
HALL_XY = (32.7, 96.9)     # TO-92 on back face, reaches z=-7.4
BATT_CONN_XY = (32.5, 91.0)  # JST-PH on back, reaches z=-4.9
MOUNT_HOLES = ((6.5, 73.9), (28.5, 73.9), (2.5, 3.4), (32.5, 3.4))  # dia 2.0 (read from the board solid)
ESP_BOX = (9.0, 76.9, 27.0, 102.7, 4.7)   # antenna end overhangs the board edge

# ---------- shell construction ----------
WALL = 2.0                 # target wall thickness
CLEARANCE = 0.8            # gap between components and inner wall

# ---------- 14500 (under the board, along Y, sliding panel on the back) ----------
BATT_D = 14.9              # 14500 (14.5 max) + tolerance
BATT_L = 50.5
BATT_HOLDER_L = 54.0       # face to face between the two contact bulkheads (cell 50.5 + 3.5 of spring travel)
BATT_Y0 = 7.5              # palm-end bulkhead face (+ flat contact); the screen-end face (spring, -) is BATT_Y0 + BATT_HOLDER_L
BATT_BACK_CLEAR = 2.6      # keep-out below board back face for back-side parts
BATT_CLEAR = 0.4           # clearance between the cell and the inner back wall (slider panel)
SLIDER_T = 1.6             # sliding panel thickness (the wall behind the cell)
BATT_AXIS_Z = -(BATT_BACK_CLEAR + BATT_D / 2)
BACK_SURFACE_Z = -(BATT_BACK_CLEAR + BATT_D + SLIDER_T)

# ---------- OLED (1.3" module, tilted toward the stick) ----------
# Panel rises toward the screen end, so its face looks back toward the thumb.
OLED_W = 35.56             # Adafruit 938 PCB width (x)
OLED_L = 33.02             # PCB length along its tilted face
OLED_T = 4.5               # stack height, header omitted, Qwiic cable (Jess, 2026-10-17)
OLED_TILT_DEG = 30.0       # tilt toward the thumb (face looks back toward the stick)
OLED_Y0 = 75.0             # low edge of the module (y)
# underside of the tilted slab clears the ESP32 shield at its near edge by 1.2 mm
OLED_BASE_Z = ESP_BOX[4] + 1.2 - (ESP_BOX[1] - OLED_Y0) * math.tan(math.radians(OLED_TILT_DEG))
OLED_HOLES = (30.48, 27.94)   # mounting hole spacing, 2.5 mm holes in the corner ears
OLED_POD_R = 5.0           # fillet radius on the pod
OLED_WINDOW = (29.42, 14.70)   # active area; bezel margin added in the cutout step

# ---------- thumbstick (GuliKit PS5 hall stick; standard gamepad values until measured) ----------
STICK_PIVOT_Z = 14.0       # pivot point above the board back face (just above the housing top, 13.5)
STICK_CAP_D = 17.5         # cap diameter
STICK_CAP_Z0 = 3.0         # cap underside above the pivot
STICK_CAP_Z1 = 7.25        # cap top above the pivot (21.25 above the back face)
STICK_TILT_DEG = 20.0      # max tilt each way
STICK_GAP = 1.2            # clearance around the swept cap

# ---------- face buttons (SW_MACRO1-6, 6x6x5 tactile switches on a 3 x 2 grid) ----------
MACRO_XS = (7.2, 17.5, 27.8)
MACRO_YS = (38.7, 47.4)
MACRO_HOLE_D = 7.0         # through hole for the printed cap (cap body 6.4, retaining flange inside)

# ---------- board capture (no fasteners: posts + pins, edge ribs) ----------
POST_R_MAX = 2.3           # post radius, reduced automatically near components
POST_R_MIN = 1.6
PIN_D = 1.7                # locating pin through the 2.0 mm mounting hole
SOCKET_D = 2.1             # matching socket in the bottom post
SOCKET_DEPTH = 3.0
PIN_BELOW = 2.0            # how far the pin reaches below the board back face
RIB_T = 1.6                # edge rib thickness (x) on the board
RIB_LEN = 5.0              # edge rib length (y)
RIB_MARGIN = 0.5           # keep ribs this far from components
RIB_SPACING = 20.0
EMBED = 0.8                # features are clipped to the shell inset by this much (they embed into the 2 mm wall)

# ---------- palm-end and side openings (printed caps; hole = cap + CAP_CLEAR per side) ----------
CAP_CLEAR = 0.25           # radial clearance between a cap neck/head and its hole
CAP_POCKET = 0.55          # depth of the head pocket in the 2 mm wall (head sits nearly flush; 1 mm of wall stays behind it)
SW_Z = 3.3                 # height of the side-actuated switch actuators above the back face (Alps, z 1.59..4.95)
SIDE_TIP_X = (-0.3, 35.3)  # actuator tips of the left / right side switches
SIDE_NECK_D = 3.4          # cap neck through the wall
SIDE_HEAD = (7.0, 9.0)     # cap head (y length, z height), stadium, bigger than the 3 mm actuator
SIDE_FLANGE_D = 5.6        # retaining flange inside, bears on the inner wall
PWR_TIP_Y = -0.1           # power switch actuator face (y), actuator at x = PWR_XY[0]
PWR_OFFSET = 4.0           # the cap head sits this far left (-x) of the switch
PWR_HEAD_D = 6.0           # cap head
PWR_NECK_D = 3.0           # cone neck (oblique, aimed at the switch)
PWR_FLANGE_D = 5.4
USB_MOUTH_Y = -0.85        # receptacle mouth (y); the plug overmold must reach about here to seat
USB_TUNNEL = (12.4, 6.6)   # plug overmold clearance (w x h); a connector-sized hole (9.4 x 3.8) cannot seat a plug
USB_SLEEVE_T = 1.2         # wall of the sleeve that carries the tunnel inward from the end wall
LED_BOSS_D = 4.2           # guide boss around the filament light pipe, inside
LED_BOSS_Z0 = 4.0          # boss bottom (LED top is 2.5 above the back face)
STRAP_D = 3.0              # cord holes in the back wall, near the palm end (cord goes down one, up the other)
STRAP_XS = (12.5, 22.5)
STRAP_Y = 3.0
STRAP_PAD = (9.0, 26.0, -0.5, 5.2, -13.5)   # x0, x1, y0, y1, z_top of the thickened pad inside the back wall

# ---------- equator split: top shell overhangs, bottom shell underhangs (half-lap), hidden interior snap clips ----------
SEAM_Z = -4.0              # seam height (the shell is widest around here; side buttons sit above it)
LAP_H = 3.0                # the top shell's outer skin reaches this far below the seam; the bottom keeps only its inner half there
LAP_UNDER = 0.90           # the bottom shell keeps this much of the wall, measured from the cavity face
LAP_GAP = 0.10             # clearance between lip and underhang (total, across the joint)
LAP_FLOOR = 0.15           # the step in the bottom shell is this much deeper than the lip, so the lip never bottoms out
CLIP_ARM_T = 1.2           # snap arm thickness (printed on the bottom shell, flexes inward)
CLIP_ARM_W = 6.0           # arm width along the wall
CLIP_Z0 = -11.0            # arm foot (a web embedded in the bottom wall)
CLIP_FOOT_H = 3.0          # web height at the foot
CLIP_OFF = 1.0             # arm's wall-side face stands this far inside the cavity face
LEDGE_Z0 = -1.0            # ledge (top shell, on its inner face): lower chamfer starts here
LEDGE_P = 0.7              # ledge protrusion into the cavity
LEDGE_H = 1.4              # ledge height
BARB_P = 0.8               # barb protrusion from the arm toward the wall (hooks over the ledge by BARB_P - CLIP_OFF + LEDGE_P)
BARB_H = 0.6               # barb flat height before the top chamfer
CLIP_FIT = 0.10            # vertical clearance between the ledge top and the barb underside
TAB_GAP = 0.2              # bottom-rib tabs above the seam keep this gap to the top wall's inner face
CLIP_CANDIDATES = (        # (wall, along-wall positions to try); 'L'/'R' are the long sides (y), 'E' the screen end (x)
    ("L", (14.0, 22.0, 30.0, 40.0, 84.0, 94.0)), ("R", (14.0, 22.0, 30.0, 40.0, 84.0, 94.0)))  # no end-wall clip: the bumper bar owns the screen-end wall
CLIP_PER_WALL = 3
CLIP_SPACING = 16.0

# ---------- battery panel: the back of the bottom shell slides out the palm end on two T-rails ----------
PANEL_X = (8.0, 27.0)      # plate width (x) at the edges; symmetric about the board centre line
PANEL_Y_END = 68.0         # far end stop (the belly is prismatic from y = 8 to here)
PANEL_Z_TOP = -7.6         # the panel stays below the lap floor (-7.15) so it can slide past the top shell's lip
PANEL_GAP = 0.2            # clearance on every sliding face
RAIL_ZT = -13.6            # tongue centre height
RAIL_T = 1.2               # tongue thickness
RAIL_GROOVE = 1.5          # groove height (tongue + 0.15 each side)
RAIL_REACH = 2.5           # tongue length into the block (x)
RAIL_BLOCK = (2.0, -18.1, -11.6)   # block: x from the wall side (x0), z bottom, z top; it ends at the panel gap
RAIL_WEB = 1.5             # web (x) that lifts the tongue off the plate
DETENT_R = 0.35            # bump on each tongue; dimple in the groove ceiling is DETENT_R + 0.05
DETENT_Y = 14.0

# ---------- small features ----------
LIGHT_PIPE_D = 1.75        # filament width
LIGHT_PIPE_CLEAR = 0.1     # the hole is this much larger
SCREW_D = 2.0

# ---------- outer skin shapes ----------
# Cross-sections are superellipses: |x/a|^n + |z/b|^n = 1, with a different
# exponent n for the front (thumb) half and the back (palm) half.
#   n = 2 is an ellipse, 3-4 is a rounded square.
# 'eq' = where the widest point sits between back (0) and front (1).
# Table rows are the ERGONOMIC MINIMUM: (y, width, z_front, z_back[, n_front, n_back]).
# Rows without n values use the shape's nf / nb. The solver in build.py grows
# each station until the electronics clear the walls.
SMOOTH_MAX_R = 1           # dilate each requirement by +-this many stations ...
SMOOTH_SIGMA = 1.3         # ... then blur by this many stations (3 mm each)
_B = -18.0
SHAPES = {
    # roundest option: pebble look everywhere the hand touches, flatter over XBee/screen
    "A": dict(
        name="Soft pebble", nf=2.6, nb=2.2, eq=0.45,
        table=[(-6.0, 20, 4, -8, 3.2, 2.2), (-3.0, 34, 9, -14, 3.2, 2.2),
               (1.0, 42, 12, -19, 3.2, 2.2), (8.0, 46, 14, _B, 3.0, 2.2),
               (24.0, 47, 14, _B, 3.0, 2.2), (40.0, 44, 12, _B, 2.5, 2.2),
               (50.0, 45, 12, _B, 2.5, 2.2), (60.0, 47, 16, _B, 2.5, 2.2),
               (74.0, 47, 16, _B, 2.6, 2.2), (84.0, 45, 15, -16, 3.0, 2.2),
               (96.0, 43, 14, -12, 3.4, 2.2), (104.0, 40, 11, -8, 3.4, 2.2),
               (109.0, 28, 6, -4, 3.2, 2.2)]),
    # round where the hand wraps (waist, stick, back), flatter over XBee and screen
    "B": dict(
        name="Gamepad grip", nf=2.8, nb=2.0, eq=0.40,
        table=[(-6.0, 22, 4, -9, 3.2, 2.0), (-3.0, 36, 9, -15, 3.2, 2.0),
               (1.0, 45, 12, -20, 3.4, 2.0), (9.0, 50, 14, _B, 3.6, 2.0),
               (24.0, 50, 14, _B, 3.6, 2.0), (40.0, 43, 12, _B, 2.4, 2.0),
               (50.0, 42, 12, _B, 2.4, 2.0), (60.0, 46, 16, _B, 2.6, 2.0),
               (74.0, 47, 16, _B, 2.8, 2.0), (84.0, 43, 14, -16, 3.4, 2.0),
               (96.0, 40, 13, -12, 3.8, 2.0), (104.0, 37, 11, -8, 3.4, 2.0),
               (109.0, 26, 6, -4, 3.0, 2.0)]),
    # flat-faced like a modern handheld: crisp front, fully round back
    "C": dict(
        name="Sculpted flat face", nf=4.0, nb=2.0, eq=0.50,
        table=[(-6.0, 22, 4, -9), (-3.0, 36, 9, -12.5), (1.0, 43, 12, -16),
               (8.0, 46, 14, _B), (24.0, 46, 14, _B), (40.0, 44, 12, _B),
               (50.0, 44, 12, _B), (60.0, 46, 16, _B), (74.0, 46, 16, _B),
               (84.0, 44, 15, -14.5), (96.0, 42, 14, -12), (104.0, 38, 11, -8),
               (109.0, 28, 6, -4)]),
}
SECTION_STEP = 3.0         # spacing of solver stations
LOFT_STEP = 1.5            # spacing of the sections actually lofted
END_ZONE = 9.0             # length of each end dome that gets fine sections
END_STEP = 0.5             # section spacing inside the end domes
SECTION_POINTS = 64        # points per section curve
SOLVER_WINDOW = 3.5        # components within +-this of a station constrain it
SOLVER_MARGIN = 0.5        # extra wall allowance for surface slope between stations

# ---------- printed caps (made by caps.py; each is printed head-down, no supports) ----------
CAP_PROUD = 0.6            # domed head apex stands this far above the outer skin (faces)
CAP_DOME_R = 9.0           # dome radius of a face-cap head
CAP_FLANGE_T = 0.8         # retaining flange thickness
CAP_FLANGE_GAP = 0.05      # flange top stops this far short of the inner wall at rest
FACE_CAP_D = 6.4           # head/body (hole is MACRO_HOLE_D = 7.0)
FACE_FLANGE_D = 8.0
FACE_PLUNGER_D = 3.5       # presses the 6x6 tact switch button
FACE_SWITCH_TOP = 6.59     # top of the tact switch button above the back face
SIDE_HEAD_PROUD = 0.4      # side-cap head stands this far out of its pocket mouth

# ---------- trigger (PS5-R2 style pendulum pad on the belly at the screen end) and bumper ----------
HINGE_PIN_D = 2.4          # printed pins on the part
HINGE_PIN_LEN = 1.9        # pin length beyond the ear
HINGE_SLOT = 2.7           # slot width (pin + 0.3)
HINGE_MOUTH = 2.15         # snap mouth (narrower than the pin by 0.25)
HINGE_FIN_T = 1.5          # fin plate thickness (x), fins stand on the shell wall
HINGE_FIN_GAP = 0.25       # fin to ear / pin tip clearance
FLAP_GAP = 0.35            # window clearance around a moving part

TRIG_X = (6.7, 28.3)       # pad width across the screen end (22 mm)
TRIG_Z_FLAT = -20.68       # flat lower lip = the belly's low line, so the controller rests on it and does not roll
TRIG_PROFILE = ((108.0, -7.6), (111.3, -10.5), (113.2, -13.8), (113.6, -17.0), (112.2, -19.7), (110.2, -20.68))  # finger face (y, z), top to bottom, at x = centre
TRIG_BACK_Y = 99.0         # the lower lip's flat runs from the finger face back to here, under the belly
TRIG_CORNER_R = 4.0        # rounded corners seen from the front
TRIG_DISH_R, TRIG_DISH_DEPTH, TRIG_DISH_Y = 30.0, 1.2, 112.0   # concave finger dish across x (vertical cylinder cut)
TRIG_GAP_BELLY = 0.4       # hook to belly gap under the shell
TRIG_GAP_DOME = 3.5        # hook to end-dome gap at rest (the pull closes about 3 mm of it)
TRIG_PIVOT = (101.0, -5.5) # hinge axis (y, z), along x, inside the shell above the belly
TRIG_THETA_MAX = 12.0      # pull travel in degrees (the hook swings toward the palm)
TRIG_STEMS = ((8.0, 10.2), (24.8, 27.0))   # two stems carry the hook through the belly to the pivot (x ranges)
TRIG_STEM_Y = (99.4, 102.6)
TRIG_STEM_Z0 = -19.0       # tenon bottom, inside the hook (the hook is glued on from below)
TRIG_TAIL_Y0 = 88.5        # tails run toward the palm over the belly (lever for the magnet and the spring)
TRIG_TAIL_Z = (-12.6, -10.6)
TRIG_TAILS_X = ((8.0, 11.0), (23.5, 27.0))
MAG_D, MAG_T = 3.0, 2.0    # neodymium disc magnet in the right tail's top face
MAG_XY = (25.0, 92.0)      # Hall sensor body must sit here: bend its leads over from the board holes at x = 32.7
SPRING_XY = (9.5, 91.0)    # post on the left tail, spring runs up to the board back face (z = 0)
SPRING_POST_D, SPRING_POST_H = 3.4, 3.2
SPRING_OD, SPRING_FREE = 4.4, 13.0

BUMP_X = (6.5, 29.5)       # bar across the screen end, over the switch's x (4.72) via a prong
BUMP_Z = (-1.0, 7.5)
BUMP_PROUD = 0.5
BUMP_INSET = 0.6           # the bar's back face sits this far inside the cavity face
BUMP_PIVOT = (103.0, 10.5) # hinge axis (y, z), along x, above the bar
BUMP_THETA_MAX = 8.0
BUMP_SWITCH = (4.72, 100.3, 3.2)   # actuator tip (x, y, z); the prong presses it
BUMP_PRONG_D = 2.4

# ---- battery bay (battery.py): cradle ribs + contact bulkheads in the fixed bottom shell, opened from below when the panel is out
BAY_CLEAR = 0.15           # radial clearance cell to cradle
BAY_RIB_T = 1.6            # cradle wall thickness (radial)
BAY_ROOF_T = 1.0           # roof over the cell (the shroud is closed on top; parts that dip into it get pockets)
BAY_POCKET_GAP = 0.25      # clearance around board parts that reach into the roof zone
BAY_ZB = -11.9             # cradle foot bottom (it lands on the rail blocks, 0.3 mm overlap)
BAY_BULK_T = 2.1           # bulkhead thickness (lip 0.4 + plate slot 0.65 + back 1.05)
BAY_LIP_T, BAY_SLOT_T = 0.4, 0.65
BAY_WINDOW_D = 8.2         # round window through the lip: flat-contact nub / spring coil reach the cell
BAY_PLATE = (10.0, 9.0, 0.5)   # stamped contact plate: width (x), height (z), thickness; generic, adjust to the real part
BAY_TONGUE = (10.0, 25.0, -14.8)   # x0, x1, z bottom of the lower tongue that holds the plate slot
BAY_TAB_W = 4.0            # solder-tab slot through the back wall
SPRING_PROJ = 5.0          # spring contact free projection beyond the lip face (solid height about 1.0)

# ---- palm end: no half-lap here, a plain flat joint at the seam (the overlap skin used to hang past the cut-away dome as a skirt with wings)
PALM_BUTT_Y = 10.0        # the lap overlap starts at this y (first clip is at 14)

# ---- palm tip: a smooth rounded cap (the old steep 22->42 mm widening made the loft overshoot into flared "ears")
TIP_Y = -5.5      # apex of the cap
TIP_W = 16.0      # width of the flat end section
TIP_ZF = 0.35     # tip section front height as a fraction of the y=0 section
TIP_ZB = 0.45     # same, back
TIP_POW = 2.4     # roundness of the blend
TIP_STEP = 0.12   # section spacing inside the cap
