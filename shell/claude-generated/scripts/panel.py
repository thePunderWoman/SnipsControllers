"""Battery panel: a section of the bottom shell's back that slides out the palm end.

The belly is prismatic from y = 8 to the end stop, so a straight rail works. Each side has a block on the fixed shell
with a groove, and a web + tongue on the panel (T-rail): the fixed material sits above and below the tongue, so the
panel is held against both pushing in and falling out, and moves only along y. A bump on each tongue clicks into a
dimple in the groove ceiling (the end clip). The panel stays below z = PANEL_Z_TOP so its end cap clears the top
shell's lip; the strap pad rides on the panel. Everything is mesh booleans (manifold3d).
"""
import numpy as np
import trimesh
import params as P
import meshops as mo

CX = P.BOARD_W / 2


def _box(x0, x1, y0, y1, z0, z1):
    b = trimesh.creation.box(extents=(x1 - x0, y1 - y0, z1 - z0))
    b.apply_translation(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    return mo.from_trimesh(b)


def _ball(x, y, z, r):
    s = trimesh.creation.icosphere(subdivisions=3, radius=r)
    s.apply_translation((x, y, z))
    return mo.from_trimesh(s)


def _mirror_x(x0, x1):
    return 2 * CX - x1, 2 * CX - x0


def split_panel(bot, inset2):
    """(fixed bottom shell, panel) from the finished bottom shell. `inset2` keeps rail parts inside the shell."""
    g = P.PANEL_GAP
    px0, px1 = P.PANEL_X
    y_lo, y_hi = -40.0, P.PANEL_Y_END
    z_lo = -45.0
    zt, rt, gr = P.RAIL_ZT, P.RAIL_T, P.RAIL_GROOVE
    sides = []
    for sgn in (1, -1):                       # left (x small) and right, mirrored about the centre line
        def sx(a, b):                         # an x interval on this side
            return (a, b) if sgn == 1 else _mirror_x(a, b)
        block_x = sx(P.RAIL_BLOCK[0], px0 - g)
        groove_x = sx(px0 - P.RAIL_REACH - 0.5, px0 + 0.1)
        web_x = sx(px0, px0 + P.RAIL_WEB)
        tongue_x = sx(px0 - P.RAIL_REACH, px0 + 0.01)
        sides.append(dict(block=block_x, groove=groove_x, web=web_x, tongue=tongue_x))

    # fixed shell: add the rail blocks, cut the panel out (with gaps), cut the grooves and detent dimples
    fixed = bot
    for s in sides:
        blk = _box(s["block"][0], s["block"][1], y_lo + 30, P.PANEL_Y_END + g, P.RAIL_BLOCK[1], P.RAIL_BLOCK[2]) ^ inset2
        fixed = fixed + blk
    fixed = fixed - _box(px0 - g - 0.037, px1 + g + 0.041, 0.0, P.PANEL_Y_END + g, z_lo, P.PANEL_Z_TOP + 0.75)
    fixed = fixed - _box(px0 - g - 0.037, px1 + g + 0.041, y_lo, 0.1, z_lo, P.PANEL_Z_TOP + g)       # at the palm tip the wall comes down to the panel (only the 0.2 mm gap), so there is no slot in the dome
    # the lap ring pinches to nothing at the palm dome tip (y ~ -4.2); the cut walls would leave a fin there, so clear that bit of ring
    # (no ring-clearing cut any more: the palm end is a butt joint at the seam, so there is no lap ring to pinch)
    for s in sides:
        fixed = fixed - _box(s["groove"][0], s["groove"][1], y_lo, P.PANEL_Y_END + g + 0.1, zt - gr / 2, zt + gr / 2)
        cx = (s["tongue"][0] + s["tongue"][1]) / 2
        fixed = fixed - _ball(cx, P.DETENT_Y, zt + gr / 2, P.DETENT_R + 0.05)

    # panel: the plate, plus a web and tongue per side, plus a detent bump on each tongue
    panel = bot ^ _box(px0, px1, y_lo, P.PANEL_Y_END, z_lo, P.PANEL_Z_TOP)
    for s in sides:
        web = _box(s["web"][0], s["web"][1], y_lo + 30, P.PANEL_Y_END, -21.0, zt + rt / 2) ^ inset2
        tongue = _box(s["tongue"][0], s["tongue"][1], y_lo + 30, P.PANEL_Y_END, zt - rt / 2, zt + rt / 2) ^ inset2
        cx = (s["tongue"][0] + s["tongue"][1]) / 2
        panel = panel + web + tongue + _ball(cx, P.DETENT_Y, zt + rt / 2, P.DETENT_R)
    return fixed, panel
