"""make_methods_figure.py -- draw course_materials/images/sharing_methods.svg:
four ways two threads can share (or not share) a cache line, as small multiples.

    python3 make_methods_figure.py [output.svg]
"""
import os
import sys

BLUE, BLUE_L, ORANGE, ORANGE_L = "#2b6cb0", "#dbe9f7", "#c05621", "#fbe3d0"
GREEN_L, GREEN, GREY, INK, MUTED = "#e3f1e6", "#2f7d4a", "#9a9a9a", "#222", "#555"
W, H = 1000, 672
PW, PH = 470, 285
out = []
a = out.append


def text(x, y, s, size=13, anchor="start", fill=INK, weight="normal"):
    a(f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}" fill="{fill}" font-weight="{weight}">{s}</text>')


def rect(x, y, w, h, fill, stroke="none", rx=0, sw=1):
    a(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')


def core(x, y, label, color, light):
    rect(x, y, 92, 44, light, color, 6, 2)
    text(x + 46, y + 19, label, 13, "middle", color, "bold")
    text(x + 46, y + 36, "and its cache", 11, "middle", MUTED)


def line(x, y, cells, label="one 64-byte cache line"):
    """A cache line of 8 eight-byte slots; cells maps slot -> (text, fill, stroke)."""
    for i in range(8):
        t, f, s = cells.get(i, ("", "#fff", "#bbb"))
        rect(x + 20 * i, y, 20, 24, f, s)
        if t:
            text(x + 20 * i + 10, y + 17, t, 12, "middle", INK, "bold")
    text(x + 80, y + 40, label, 11, "middle", MUTED)


def arrow(x1, x2, y, label):
    a(f'<path d="M{x1},{y} C{x1 + 30},{y - 34} {x2 - 30},{y - 34} {x2},{y}" fill="none" stroke="#333" '
      f'stroke-width="1.8" marker-start="url(#tip)" marker-end="url(#tip)"/>')
    text((x1 + x2) / 2, y - 30, label, 11, "middle", INK)


def lanes(x, y, a_ticks, b_ticks, owner=None, length=430):
    """A timeline: lane A, lane B, and optionally who owns the line."""
    text(x - 6, y + 11, "A", 12, "end", BLUE, "bold")
    text(x - 6, y + 29, "B", 12, "end", ORANGE, "bold")
    a(f'<line x1="{x}" y1="{y + 7}" x2="{x + length}" y2="{y + 7}" stroke="#e4e3df"/>')
    a(f'<line x1="{x}" y1="{y + 25}" x2="{x + length}" y2="{y + 25}" stroke="#e4e3df"/>')
    for t0, w in a_ticks:
        rect(x + t0, y, w, 14, BLUE, rx=1)
    for t0, w in b_ticks:
        rect(x + t0, y + 18, w, 14, ORANGE, rx=1)
    yy = y + 36
    if owner:
        text(x - 6, yy + 9, "line", 10, "end", MUTED)
        for t0, w, who in owner:
            w = min(w, length - t0)
            if w <= 0:
                continue
            f = {"A": BLUE_L, "B": ORANGE_L, "moving": "#fff"}[who]
            s = {"A": BLUE, "B": ORANGE, "moving": GREY}[who]
            rect(x + t0, yy, w, 11, f, s)
        yy += 15
    a(f'<line x1="{x}" y1="{yy + 4}" x2="{x + length}" y2="{yy + 4}" stroke="#888" marker-end="url(#tip)"/>')
    text(x + length, yy + 17, "time", 10, "end", MUTED)


def panel(px, py, n, title, mode):
    rect(px, py, PW, PH, "#fcfcfb", "#ddd", 8)
    a(f'<circle cx="{px + 22}" cy="{py + 24}" r="11" fill="#333"/>')
    text(px + 22, py + 29, n, 13, "middle", "#fff", "bold")
    text(px + 42, py + 29, title, 15, "start", INK, "bold")
    text(px + PW - 14, py + 29, mode, 11, "end", MUTED)


def caption(px, py, lines):
    for i, s in enumerate(lines):
        text(px + 18, py + PH - 38 + 17 * i, s, 12, "start", INK)


a(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img" '
  'aria-label="Four ways two threads can use one cache line. 1, true sharing: both threads use one '
  'variable and take turns; the line moves on every write and each write waits for the last, so the '
  'transfers happen one after another. 2, false sharing: each thread writes its own variable, but both '
  'are in one line; writes wait in each core\'s store buffer and commit in batches, so the line moves '
  'now and then. 3, false sharing with a fence or an atomic: each write must own the line before the '
  'next can start, so the line moves for nearly every write. 4, padded: each variable on its own line; '
  'nothing moves.">')
a('<defs><marker id="tip" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" '
  'orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="#333"/></marker></defs>')
rect(0, 0, W, H, "#fff")
a('<g font-family="Helvetica, Arial, sans-serif">')
text(W / 2, 30, "Four ways two threads can use one cache line", 19, "middle", INK, "bold")
text(W / 2, 52, "Thread A (blue) and thread B (orange) run on different cores. Each block on a timeline is one write.",
     13, "middle", MUTED)

cols, rows = [20, 510], [70, 375]

# ---- 1: true sharing --------------------------------------------------------------
px, py = cols[0], rows[0]
panel(px, py, "1", "True sharing: ping-pong", "modes pause, spin")
core(px + 18, py + 76, "thread A", BLUE, BLUE_L)
core(px + PW - 110, py + 76, "thread B", ORANGE, ORANGE_L)
line(px + 155, py + 86, {0: ("x", GREEN_L, GREEN)})
arrow(px + 112, px + PW - 112, py + 82, "moves on every write")
ta, tb, own, t = [], [], [], 0
for k in range(6):
    who = "A" if k % 2 == 0 else "B"
    (ta if who == "A" else tb).append((t, 8))
    own.append((t, 8, who))
    own.append((t + 8, 62, "moving"))
    t += 70
lanes(px + 30, py + 150, ta, tb, own)
caption(px, py, ["Both threads use one variable and take turns. Each write waits",
                 "for the other thread's, so every transfer is paid in full, in turn."])

# ---- 2: false sharing ------------------------------------------------------------
px, py = cols[1], rows[0]
panel(px, py, "2", "False sharing", "mode false")
core(px + 18, py + 76, "thread A", BLUE, BLUE_L)
core(px + PW - 110, py + 76, "thread B", ORANGE, ORANGE_L)
line(px + 155, py + 86, {3: ("a", BLUE_L, BLUE), 4: ("b", ORANGE_L, ORANGE)})
arrow(px + 112, px + PW - 112, py + 82, "moves now and then")
ta = [(i * 5, 3) for i in range(86)]
tb = [(i * 5 + 2, 3) for i in range(86)]
own, t, k = [], 0, 0
while t < 430:
    who = "A" if k % 2 == 0 else "B"
    own.append((t, min(84, 430 - t), who))
    if t + 84 < 430:
        own.append((t + 84, 8, "moving"))
    t += 92
    k += 1
lanes(px + 30, py + 150, ta, tb, own)
caption(px, py, ["Each thread writes only its own variable. Writes wait in the core's",
                 "store buffer and go in batches whenever it holds the line."])

# ---- 3: false sharing, each write completes ---------------------------------------------
px, py = cols[0], rows[1]
panel(px, py, "3", "False sharing, every write completes", "false-fence, false-atomic")
core(px + 18, py + 76, "thread A", BLUE, BLUE_L)
core(px + PW - 110, py + 76, "thread B", ORANGE, ORANGE_L)
line(px + 155, py + 86, {3: ("a", BLUE_L, BLUE), 4: ("b", ORANGE_L, ORANGE)})
arrow(px + 112, px + PW - 112, py + 82, "moves for nearly every write")
ta, tb, own, t, k = [], [], [], 0, 0
while t < 430:
    who = "A" if k % 2 == 0 else "B"
    (ta if who == "A" else tb).append((t, 8))
    own.append((t, 8, who))
    if t + 8 < 430:
        own.append((t + 8, 22, "moving"))
    t += 30
    k += 1
lanes(px + 30, py + 150, ta, tb, own)
caption(px, py, ["A fence or an atomic makes each write own the line before the",
                 "next can start, so the line moves for nearly every write."])

# ---- 4: padded --------------------------------------------------------------------------
px, py = cols[1], rows[1]
panel(px, py, "4", "Padded: nothing shared", "padded, padded-fence, padded-atomic")
core(px + 18, py + 76, "thread A", BLUE, BLUE_L)
core(px + PW - 110, py + 76, "thread B", ORANGE, ORANGE_L)
line(px + 155, py + 72, {0: ("a", BLUE_L, BLUE)}, "")
line(px + 155, py + 102, {0: ("b", ORANGE_L, ORANGE)}, "two cache lines, one each")
ta = [(i * 5, 3) for i in range(86)]
tb = [(i * 5 + 2, 3) for i in range(86)]
lanes(px + 30, py + 162, ta, tb)
caption(px, py, ["Each variable on its own line: each core keeps its line, and",
                 "nothing moves. This is what an update costs with no sharing."])

a("</g></svg>")
here = os.path.dirname(os.path.abspath(__file__))
dst = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "course_materials", "images", "sharing_methods.svg")
open(dst, "w").write("\n".join(out) + "\n")
print(dst)
