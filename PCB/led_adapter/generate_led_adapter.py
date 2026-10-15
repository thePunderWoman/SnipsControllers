#!/usr/bin/env python3
"""Generate the D_RGB1 LED adapter board and its 4x4 panel.

The 15 rev-1 Snips boards (10 + Alex's 5) were fabbed with the old
SK6812MINI-E-012 land pattern at D_RGB1. That land was drawn from the
datasheet's BOTTOM view, so a lens-up LED is mirrored on it and no rotation
fixes it. This adapter solders onto that old land with its bottom pads and
carries the current SK6812MINI-HS-012 top-view land (the same footprint the
main board now uses) on its top.

Coordinates are millimetres, KiCad orientation (y grows down), with the
origin at D_RGB1's centre on the rev-1 board. Seen from above, the adapter's
bottom pads sit exactly over the old pads, so they use the old footprint's
coordinates unchanged:

    old pad 1 GND  (-2.27, +1.06)  lower-left
    old pad 2 DIN  (-2.27, -1.06)  upper-left
    old pad 3 3V3  (+2.27, -1.06)  upper-right
    old pad 4 n/c  (+2.27, +1.06)  lower-right, no trace on the main board

USB-C (J_USB1) is off the -x edge of the adapter, which is what the silkscreen
arrows point at.

Run with KiCad's bundled Python (it needs the pcbnew module):

    /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \\
        PCB/led_adapter/generate_led_adapter.py

It writes led_adapter.kicad_pcb (one adapter) and led_adapter_panel.kicad_pcb
(the 4x4 panel to order) next to this script. Do not edit those by hand.
"""

import os

import pcbnew
from pcbnew import FromMM, ToMM

HERE = os.path.dirname(os.path.abspath(__file__))
LIB = os.path.join(HERE, "..", "libraries", "SnipsControllers_Custom.pretty")

# Adapter outline (half sizes). Bounded by the rev-1 board around D_RGB1:
# J_USB1 courtyard at x=-3.71, MH4 courtyard at x=+3.67, R_RGB1/C_RGB1/R_CC2
# courtyards at y=-2.94, board edge at y=+3.275.
HALF_W = 3.4
HALF_H = 2.8

# Old SK6812MINI-E-012 land, as etched on the rev-1 boards (pad, net, x, y).
MATE_PAD_SIZE = (1.34, 0.68)
MATE_PADS = [
    ("1", "GND", -2.27, 1.06),
    ("2", "DIN", -2.27, -1.06),
    ("3", "3V3", 2.27, -1.06),
    ("4", None, 2.27, 1.06),
]

# SK6812MINI-HS-012 footprint pad -> net (pad 4 DOUT is unused).
LED_NETS = {"1": "GND", "2": "DIN", "3": "3V3", "4": None}

TRACK_W = 0.25
VIA_D = 0.6
VIA_DRILL = 0.3

# (net, layer, points); vias at (net, x, y).
TRACKS = [
    ("GND", "B", [(-2.27, 1.06), (-2.27, 1.9), (1.15, 1.9)]),
    ("GND", "F", [(1.15, 1.9), (1.15, 0.9)]),
    ("DIN", "B", [(-2.27, -1.06), (-2.27, -1.9), (1.15, -1.9)]),
    ("DIN", "F", [(1.15, -1.9), (1.15, -0.9)]),
    ("3V3", "B", [(2.27, -1.06), (2.27, 0.0), (0.0, 0.0)]),
    ("3V3", "F", [(0.0, 0.0), (-1.65, -0.9)]),
]
VIAS = [("GND", 1.15, 1.9), ("DIN", 1.15, -1.9), ("3V3", 0.0, 0.0)]

# Silkscreen. The top strips (|y| > 1.9) stay clear of the LED body
# (3.7 x 3.5 mm, datasheet section 4), so they remain visible once it is on.
# Arrows on both sides point at the USB-C connector (-x).
TOP_ARROW = [(-3.15, -2.3), (-2.45, -2.63), (-2.45, -2.4), (-1.85, -2.4),
             (-1.85, -2.2), (-2.45, -2.2), (-2.45, -1.97)]
BOTTOM_ARROW = [(-3.15, -2.15), (-2.35, -2.6), (-2.35, -2.27), (-1.75, -2.27),
                (-1.75, -2.03), (-2.35, -2.03), (-2.35, -1.7)]
# (text, layer, x, y, height)
SILK_TEXT = [
    ("USB", "F", -0.75, -2.3, 0.6),
    ("TOP", "F", -2.3, 2.3, 0.6),
    ("USB", "B", -0.3, -2.15, 0.8),
    ("BOTTOM", "B", -1.25, 2.15, 0.7),
    ("NC", "B", 0.85, 1.06, 0.5),
]

# Panel: 4x4 = 16 adapters (15 needed + 1 spare), held by one mouse-bite tab
# on each x side.
NX, NY = 4, 4
GAP = 2.0
RAIL = 5.0
TAB_W = 2.2
BITE_DRILL = 0.5
BITE_YS = (-0.75, 0.0, 0.75)
BITE_OFFSET = 0.1  # hole centres sit just outside the adapter edge
ORIGIN = (100.0, 100.0)


def pt(x, y):
    return pcbnew.VECTOR2I(FromMM(x), FromMM(y))


def layer(side, kind):
    return getattr(pcbnew, ("F_" if side == "F" else "B_") + kind)


def new_board():
    board = pcbnew.BOARD()
    ds = board.GetDesignSettings()
    ds.m_CopperEdgeClearance = FromMM(0.3)
    ds.m_MinSilkTextHeight = FromMM(0.5)
    ds.m_TentViasFront = True
    ds.m_TentViasBack = True
    return board


def get_net(board, name):
    net = board.FindNet(name)
    if net is None:
        net = pcbnew.NETINFO_ITEM(board, name)
        board.Add(net)
    return net


def add_poly(board, pts, lyr, filled, width):
    shape = pcbnew.PCB_SHAPE(board)
    shape.SetShape(pcbnew.SHAPE_T_POLY)
    shape.SetPolyPoints([pt(x, y) for x, y in pts])
    shape.SetLayer(lyr)
    shape.SetFilled(filled)
    shape.SetWidth(FromMM(width))
    board.Add(shape)


def add_text(board, text, lyr, x, y, height, mirrored):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetLayer(lyr)
    t.SetTextSize(pcbnew.VECTOR2I(FromMM(height), FromMM(height)))
    t.SetTextThickness(FromMM(0.12))
    t.SetPosition(pt(x, y))
    t.SetMirrored(mirrored)
    board.Add(t)


def add_adapter(board, n, ox, oy, net_prefix=""):
    """Add adapter number n with its centre at (ox, oy)."""

    def net(name):
        return get_net(board, net_prefix + name)

    led = pcbnew.FootprintLoad(LIB, "SK6812MINI-HS-012")
    # Unlinked from the library: the silk edit below would otherwise show as a
    # library mismatch in DRC. Pads are the library's, unchanged.
    led.SetFPID(pcbnew.LIB_ID("", "SK6812MINI-HS-012"))
    led.SetReference(f"D{n}")
    led.GetField(pcbnew.FIELD_T_REFERENCE).SetVisible(False)
    # Move the body-outline silk lines to F.Fab; the adapter silk uses that
    # space. (Removing them through the SWIG API corrupts the footprint.)
    for item in led.GraphicalItems():
        if item.GetLayer() == pcbnew.F_SilkS and item.GetShape() == pcbnew.SHAPE_T_SEGMENT:
            item.SetLayer(pcbnew.F_Fab)
    board.Add(led)
    led.SetPosition(pt(ox, oy))
    for pad in led.Pads():
        if LED_NETS[pad.GetNumber()]:
            pad.SetNet(net(LED_NETS[pad.GetNumber()]))

    # Bottom pads: build them front-side at mirrored x, then flip the
    # footprint to the back, which mirrors x back to the old land's positions.
    mate = pcbnew.FOOTPRINT(board)
    mate.SetFPID(pcbnew.LIB_ID("", "SK6812MINI-E-012_mate"))
    mate.SetReference(f"J{n}")
    mate.GetField(pcbnew.FIELD_T_REFERENCE).SetVisible(False)
    mate.SetValue("SK6812MINI-E-012_mate")
    mate.GetField(pcbnew.FIELD_T_VALUE).SetVisible(False)
    mate.SetAttributes(pcbnew.FP_SMD | pcbnew.FP_EXCLUDE_FROM_BOM | pcbnew.FP_EXCLUDE_FROM_POS_FILES)
    board.Add(mate)
    mate.SetPosition(pt(ox, oy))
    for number, net_name, x, y in MATE_PADS:
        pad = pcbnew.PAD(mate)
        pad.SetNumber(number)
        pad.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
        pad.SetLayerSet(pad.SMDMask())
        pad.SetShape(pcbnew.PAD_SHAPE_ROUNDRECT)
        pad.SetSize(pt(*MATE_PAD_SIZE))
        pad.SetRoundRectRadiusRatio(0.25)
        pad.SetFPRelativePosition(pt(-x, y))
        mate.Add(pad)
        if net_name:
            pad.SetNet(net(net_name))
    mate.Flip(mate.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
    for number, _, x, y in MATE_PADS:
        pos = mate.FindPadByNumber(number).GetPosition()
        assert (round(ToMM(pos.x) - ox, 3), round(ToMM(pos.y) - oy, 3)) == (x, y), number

    for net_name, side, pts in TRACKS:
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            track = pcbnew.PCB_TRACK(board)
            track.SetStart(pt(ox + x0, oy + y0))
            track.SetEnd(pt(ox + x1, oy + y1))
            track.SetWidth(FromMM(TRACK_W))
            track.SetLayer(layer(side, "Cu"))
            track.SetNet(net(net_name))
            board.Add(track)
    for net_name, x, y in VIAS:
        via = pcbnew.PCB_VIA(board)
        via.SetPosition(pt(ox + x, oy + y))
        via.SetWidth(FromMM(VIA_D))
        via.SetDrill(FromMM(VIA_DRILL))
        via.SetNet(net(net_name))
        board.Add(via)

    add_poly(board, [(ox + x, oy + y) for x, y in TOP_ARROW], pcbnew.F_SilkS, True, 0.05)
    add_poly(board, [(ox + x, oy + y) for x, y in BOTTOM_ARROW], pcbnew.B_SilkS, True, 0.05)
    for text, side, x, y, height in SILK_TEXT:
        add_text(board, text, layer(side, "SilkS"), ox + x, oy + y, height, side == "B")


def rect(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def poly_set(rects):
    polys = pcbnew.SHAPE_POLY_SET()
    for r in rects:
        chain = pcbnew.SHAPE_LINE_CHAIN()
        for x, y in r:
            chain.Append(FromMM(x), FromMM(y))
        chain.SetClosed(True)
        polys.AddOutline(chain)
    polys.Simplify()
    return polys


def build_single(path):
    board = new_board()
    ox, oy = ORIGIN
    add_adapter(board, 1, ox, oy)
    add_poly(board, rect(ox - HALF_W, oy - HALF_H, ox + HALF_W, oy + HALF_H),
             pcbnew.Edge_Cuts, False, 0.05)
    board.Save(path)


def build_panel(path):
    board = new_board()
    x0, y0 = ORIGIN
    pitch_x, pitch_y = 2 * HALF_W + GAP, 2 * HALF_H + GAP
    width = 2 * (RAIL + GAP) + NX * 2 * HALF_W + (NX - 1) * GAP
    height = 2 * (RAIL + GAP) + NY * 2 * HALF_H + (NY - 1) * GAP

    keep, bites = [], []
    for j in range(NY):
        for i in range(NX):
            cx = x0 + RAIL + GAP + HALF_W + i * pitch_x
            cy = y0 + RAIL + GAP + HALF_H + j * pitch_y
            n = j * NX + i + 1
            add_adapter(board, n, cx, cy, net_prefix=f"/A{n}/")
            keep.append(rect(cx - HALF_W, cy - HALF_H, cx + HALF_W, cy + HALF_H))
            # Tab to the left of every adapter, plus one to the right rail.
            left = cx - HALF_W - GAP
            keep.append(rect(left, cy - TAB_W / 2, cx - HALF_W, cy + TAB_W / 2))
            bites += [(cx - HALF_W - BITE_OFFSET, cy + y) for y in BITE_YS]
            bites += [(left + BITE_OFFSET, cy + y) for y in BITE_YS]
            if i == NX - 1:
                right = cx + HALF_W + GAP
                keep.append(rect(cx + HALF_W, cy - TAB_W / 2, right, cy + TAB_W / 2))
                bites += [(cx + HALF_W + BITE_OFFSET, cy + y) for y in BITE_YS]
                bites += [(right - BITE_OFFSET, cy + y) for y in BITE_YS]

    add_poly(board, rect(x0, y0, x0 + width, y0 + height), pcbnew.Edge_Cuts, False, 0.05)
    cut = poly_set([rect(x0 + RAIL, y0 + RAIL, x0 + width - RAIL, y0 + height - RAIL)])
    cut.BooleanSubtract(poly_set(keep))
    for k in range(cut.OutlineCount()):
        assert cut.HoleCount(k) == 0
        chain = cut.Outline(k)
        pts = [chain.CPoint(m) for m in range(chain.PointCount())]
        add_poly(board, [(ToMM(p.x), ToMM(p.y)) for p in pts], pcbnew.Edge_Cuts, False, 0.05)

    holes = pcbnew.FOOTPRINT(board)
    holes.SetFPID(pcbnew.LIB_ID("", "MouseBites"))
    holes.SetReference("MB1")
    holes.GetField(pcbnew.FIELD_T_REFERENCE).SetVisible(False)
    holes.GetField(pcbnew.FIELD_T_VALUE).SetVisible(False)
    holes.SetAttributes(pcbnew.FP_EXCLUDE_FROM_BOM | pcbnew.FP_EXCLUDE_FROM_POS_FILES)
    board.Add(holes)
    holes.SetPosition(pt(x0, y0))
    for x, y in bites:
        pad = pcbnew.PAD(holes)
        pad.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
        pad.SetLayerSet(pad.UnplatedHoleMask())
        pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
        pad.SetSize(pt(BITE_DRILL, BITE_DRILL))
        pad.SetDrillSize(pt(BITE_DRILL, BITE_DRILL))
        pad.SetPosition(pt(x, y))
        holes.Add(pad)

    add_text(board, "SNIPS D_RGB1 LED ADAPTER v1  -  16 UP", pcbnew.F_SilkS,
             x0 + width / 2, y0 + RAIL / 2, 1.2, False)
    board.Save(path)


if __name__ == "__main__":
    build_single(os.path.join(HERE, "led_adapter.kicad_pcb"))
    build_panel(os.path.join(HERE, "led_adapter_panel.kicad_pcb"))
