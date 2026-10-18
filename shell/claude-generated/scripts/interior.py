"""Board capture inside the shell: posts with locating pins, and edge ribs. No fasteners.

Top features (belong to the top shell) press the board's FRONT face; bottom features (bottom shell) support its
BACK face. Everything is made oversize, then clipped to the shell inset by EMBED so it merges into the 2 mm wall
without poking through. Positions avoid every component envelope (env from build.pcb_envelopes).
"""
import math
import params as P
from build123d import Box, Cylinder, Pos, Rot, Location, loft

CX = P.BOARD_W / 2
BOARD_FRONT = P.BOARD_T          # z of the front face (1.51); back face is z = 0
TOP_Z_MAX = 24.0                 # oversize top end, clipped by the inset skin
BOT_Z_MIN = -30.0                # oversize bottom end


def _is_board_plate(e):
    return (e["x1"] - e["x0"]) > 30 and (e["y1"] - e["y0"]) > 90


def _footprint_dist(x, y, e):
    return math.hypot(max(e["x0"] - x, 0.0, x - e["x1"]), max(e["y0"] - y, 0.0, y - e["y1"]))


def post_radius(x, y, z_lo, z_hi, env, margin=0.4):
    """Largest post radius at (x, y) that keeps `margin` to every component box overlapping z_lo..z_hi."""
    r = P.POST_R_MAX
    for e in env:
        if _is_board_plate(e) or e["z1"] < z_lo or e["z0"] > z_hi:
            continue
        r = min(r, _footprint_dist(x, y, e) - margin)
    return r


def free_windows(x_lo, x_hi, z_lo, z_hi, env, y_from=6.0, y_to=98.0):
    """y windows of length RIB_LEN where nothing but the board sits in the strip x_lo..x_hi, z_lo..z_hi."""
    blockers = [e for e in env if not _is_board_plate(e) and e["z1"] > z_lo and e["z0"] < z_hi
                and e["x1"] > x_lo - P.RIB_MARGIN and e["x0"] < x_hi + P.RIB_MARGIN]
    wins, y, last = [], y_from, -1e9
    while y + P.RIB_LEN <= y_to:
        a, b = y - P.RIB_MARGIN, y + P.RIB_LEN + P.RIB_MARGIN
        if y - last >= P.RIB_SPACING and not any(e["y1"] > a and e["y0"] < b for e in blockers):
            wins.append(y)
            last = y
            y += P.RIB_LEN
        else:
            y += 1.0
    return wins


def features(env):
    """(top, bottom, info): oversize unclipped solids for each shell, plus a record of what was placed."""
    top, bot, info = [], [], []
    for (x, y) in P.MOUNT_HOLES:
        r = post_radius(x, y, BOARD_FRONT, TOP_Z_MAX, env)
        if r >= P.POST_R_MIN:
            post = Pos(x, y, (BOARD_FRONT + TOP_Z_MAX) / 2) * Cylinder(r, TOP_Z_MAX - BOARD_FRONT)
            pin_len = BOARD_FRONT + P.PIN_BELOW
            pin = Pos(x, y, BOARD_FRONT - pin_len / 2) * Cylinder(P.PIN_D / 2, pin_len)
            top.append(post + pin)
            info.append(dict(name="top_post", x=x, y=y, r=round(r, 2), z0=-P.PIN_BELOW, z1=TOP_Z_MAX))
        else:
            # no room for a post (the stick body is too close): a thin arm from the side wall to a bare pin
            info.append(dict(name="top_post_skipped", x=x, y=y, r=round(r, 2)))
            left = x < P.BOARD_W / 2
            ax0, ax1 = (-8.0, x) if left else (x, P.BOARD_W + 8.0)
            t = 1.6                                   # arm thickness; offset away from the screen module's low edge
            ya0, ya1 = y - t + 0.3, y + 0.3
            # L-shaped: full height only outside the screen module's x range, low (under the module) inside it
            ox0, ox1 = CX - P.OLED_W / 2 - 0.8, CX + P.OLED_W / 2 + 0.8
            tall = (ax0, min(ax1, ox0)) if left else (max(ax0, ox1), ax1)
            low = (min(ax1, ox0) if left else ax0, ax1 if left else max(ax0, ox1))
            LOW_Z = 4.0
            blockers = [e for e in env if not _is_board_plate(e) and e.get("name") != "oled" and e["z1"] > BOARD_FRONT
                        and e["y1"] > ya0 - 0.2 and e["y0"] < ya1 + 0.2]
            clash = any(e["x1"] > tall[0] and e["x0"] < tall[1] for e in blockers) or \
                any(e["x1"] > low[0] and e["x0"] < low[1] and e["z0"] < LOW_Z for e in blockers)
            if not clash:
                pin_len = BOARD_FRONT + P.PIN_BELOW
                yc = (ya0 + ya1) / 2
                arm = Pos((tall[0] + tall[1]) / 2, yc, (BOARD_FRONT + TOP_Z_MAX) / 2) * Box(tall[1] - tall[0], t, TOP_Z_MAX - BOARD_FRONT)
                arm = arm + Pos((low[0] + low[1]) / 2, yc, (BOARD_FRONT + LOW_Z) / 2) * Box(low[1] - low[0], t, LOW_Z - BOARD_FRONT)
                pin = Pos(x, y, BOARD_FRONT - pin_len / 2) * Cylinder(P.PIN_D / 2, pin_len)
                top.append(arm + pin)
                info.append(dict(name="top_arm", x0=ax0, x1=ax1, y0=ya0, y1=ya1, z0=-P.PIN_BELOW, z1=TOP_Z_MAX,
                                 tall=list(tall), low=list(low), low_z=LOW_Z))
            else:
                info.append(dict(name="top_arm_blocked", x=x, y=y))
        rb = post_radius(x, y, BOT_Z_MIN, 0.0, env)
        if rb >= P.POST_R_MIN:
            post = Pos(x, y, (BOT_Z_MIN + 0.0) / 2) * Cylinder(rb, -BOT_Z_MIN)
            sock = Pos(x, y, -P.SOCKET_DEPTH / 2) * Cylinder(P.SOCKET_D / 2, P.SOCKET_DEPTH)
            bot.append(post - sock)
            info.append(dict(name="bottom_post", x=x, y=y, r=round(rb, 2), z0=BOT_Z_MIN, z1=0.0))
        else:
            info.append(dict(name="bottom_post_skipped", x=x, y=y, r=round(rb, 2)))
    # edge ribs: on the board edge strips, top ones press the front face, bottom ones carry the back face
    left, right = (0.3, 0.3 + P.RIB_T), (P.BOARD_W - 0.3 - P.RIB_T, P.BOARD_W - 0.3)
    for side, (xa, xb) in (("L", left), ("R", right)):
        for y0 in free_windows(xa, xb, BOARD_FRONT, TOP_Z_MAX, env):
            top.append(Pos((xa + xb) / 2, y0 + P.RIB_LEN / 2, (BOARD_FRONT + TOP_Z_MAX) / 2) *
                       Box(P.RIB_T, P.RIB_LEN, TOP_Z_MAX - BOARD_FRONT))
            info.append(dict(name="top_rib_" + side, x0=xa, x1=xb, y0=y0, y1=y0 + P.RIB_LEN, z0=BOARD_FRONT, z1=TOP_Z_MAX))
        # bottom ribs reach from the side wall (oversize outward) to 2 mm under the board edge
        bx0, bx1 = (-8.0, 2.2) if side == "L" else (P.BOARD_W - 2.2, P.BOARD_W + 8.0)
        for y0 in free_windows(bx0, bx1, BOT_Z_MIN, 0.0, env):
            bot.append(Pos((bx0 + bx1) / 2, y0 + P.RIB_LEN / 2, BOT_Z_MIN / 2) * Box(bx1 - bx0, P.RIB_LEN, -BOT_Z_MIN))
            info.append(dict(name="bottom_rib_" + side, x0=bx0, x1=bx1, y0=y0, y1=y0 + P.RIB_LEN, z0=BOT_Z_MIN, z1=0.0))
    return top, bot, info


def stadium_y(cx, cz, w, h, y0, y1):
    """Stadium (w wide in x, h high in z, w >= h) extruded along y from y0 to y1."""
    L = y1 - y0
    body = Pos(cx, (y0 + y1) / 2, cz) * Box(w - h, L, h)
    for sx in (-1, 1):
        body = body + Pos(cx + sx * (w - h) / 2, (y0 + y1) / 2, cz) * (Rot(X=90) * Cylinder(h / 2, L))
    return body


def stadium_x(cy, cz, wy, hz, x0, x1):
    """Stadium (wy long in y, hz high in z, hz >= wy) extruded along x from x0 to x1."""
    L = x1 - x0
    body = Pos((x0 + x1) / 2, cy, cz) * Box(L, wy, hz - wy)
    for sz in (-1, 1):
        body = body + Pos((x0 + x1) / 2, cy, cz + sz * (hz - wy) / 2) * (Rot(Y=90) * Cylinder(wy / 2, L))
    return body


def opening_features():
    """(top, bottom) solids that carry the openings: USB sleeve, LED guide boss, strap pad. Oversize, clipped later."""
    top, bot = [], []
    w, h = P.USB_TUNNEL
    t = P.USB_SLEEVE_T
    usb_z = 2.8
    top.append(stadium_y(P.USB_XY[0], usb_z, w + 2 * t, h + 2 * t, -12.0, P.USB_MOUTH_Y - 0.1))
    x, y = P.LED_XY
    top.append(Pos(x, y, (P.LED_BOSS_Z0 + TOP_Z_MAX) / 2) * Cylinder(P.LED_BOSS_D / 2, TOP_Z_MAX - P.LED_BOSS_Z0))
    x0, x1, y0, y1, zt = P.STRAP_PAD
    bot.append(Pos((x0 + x1) / 2, (y0 + y1) / 2, (BOT_Z_MIN + zt) / 2) * Box(x1 - x0, y1 - y0, zt - BOT_Z_MIN))
    return top, bot


def fuse(parts):
    out = parts[0]
    for p in parts[1:]:
        out = out + p
    return out
