"""Generate the SVG figures for Lectures 10-12, in the style of threads.svg.

    python3 make_lecture_figs.py .      # writes the eight SVGs into this directory

The schedule timelines are simulated, not drawn by hand: change `speed` or the
chunk size in schedule_figs() and the figures follow.
"""
import html, os, sys

OUT = sys.argv[1]
BLUE, ORANGE, GREEN, PURPLE, GRAY, INK = "#2b6cb0", "#c05621", "#2f855a", "#6b46c1", "#555", "#222"
TINT = {BLUE: "#dbe9f7", ORANGE: "#fbe3d0", GREEN: "#e3f1e6", PURPLE: "#e9e3f7", GRAY: "#eeeeee"}
THREAD = [BLUE, ORANGE, GREEN, PURPLE]


def esc(s):
    return html.escape(str(s), quote=True)


class Fig:
    def __init__(self, w, h, title, aria):
        self.w, self.h = w, h
        self.parts = []
        self.markers = set()
        self.title, self.aria = title, aria

    def add(self, s):
        self.parts.append(s)

    def text(self, x, y, s, size=13, anchor="middle", color=INK, bold=False, italic=False):
        w = ' font-weight="bold"' if bold else ""
        it = ' font-style="italic"' if italic else ""
        self.add(f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" text-anchor="{anchor}" fill="{color}"{w}{it}>{esc(s)}</text>')

    def rect(self, x, y, w, h, fill="#fff", stroke=GRAY, sw=1.3, rx=0, dash=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"{d}/>')

    def line(self, x1, y1, x2, y2, color=GRAY, sw=1.5, arrow=False, dash=None):
        m = ""
        if arrow:
            self.markers.add(color)
            m = f' marker-end="url(#m{color[1:]})"'
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{color}" stroke-width="{sw}"{d}{m}/>')

    def path(self, d, color=GRAY, sw=1.5, arrow=False, dash=None):
        m = ""
        if arrow:
            self.markers.add(color)
            m = f' marker-end="url(#m{color[1:]})"'
        ds = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{sw}"{ds}{m}/>')

    def save(self, name):
        defs = "".join(
            f'<marker id="m{c[1:]}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="{c}"/></marker>'
            for c in sorted(self.markers))
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" viewBox="0 0 {self.w} {self.h}" role="img" aria-label="{esc(self.aria)}">\n'
               f'  <defs>{defs}</defs>\n'
               f'  <rect width="{self.w}" height="{self.h}" fill="#fff"/>\n'
               f'  <g font-family="Helvetica, Arial, sans-serif" fill="{INK}">\n'
               f'    <text x="{self.w/2:.0f}" y="32" font-size="19" font-weight="bold" text-anchor="middle">{esc(self.title)}</text>\n    '
               + "\n    ".join(self.parts) + "\n  </g>\n</svg>\n")
        with open(os.path.join(OUT, name), "w") as f:
            f.write(svg)


# ---------------------------------------------------------------- Lecture 10
def fork_join():
    f = Fig(1000, 400, "Fork and join",
            "The main thread runs serially, forks a team of four threads at a parallel region, and every thread runs the region. "
            "Threads that finish early wait at the barrier at the end of the region. Then only the main thread continues.")
    x0, xf, xj, x1 = 40, 230, 800, 960
    ys = [120, 175, 230, 285]
    ends = [790, 640, 790, 520]          # where each thread's work ends
    ymain = ys[0]
    # main thread before and after
    f.line(x0, ymain, xf, ymain, INK, 4)
    f.line(xj, ymain, x1, ymain, INK, 4, arrow=True)
    f.text((x0 + xf) / 2, ymain - 14, "main thread", 13, color=INK)
    f.text((xj + x1) / 2, ymain - 14, "main thread", 13, color=INK)
    # fork lines
    for y in ys[1:]:
        f.path(f"M {xf} {ymain} C {xf+25} {ymain}, {xf+15} {y}, {xf+45} {y}", GRAY, 1.5)
    # threads
    for k, (y, e) in enumerate(zip(ys, ends)):
        c = THREAD[k]
        xs = xf + 45 if k else xf
        f.rect(xs, y - 13, e - xs, 26, TINT[c], c, 1.5, 4)
        f.text(xs + 12, y + 5, f"thread {k}" + (" (the main thread)" if k == 0 else ""), 13, "start", c, bold=True)
        if e < xj - 12:
            f.line(e + 4, y, xj - 6, y, GRAY, 1.5, dash="5 4")
            f.text((e + xj) / 2, y - 8, "waits", 12, color=GRAY, italic=True)
    # barrier
    f.line(xj, 90, xj, 310, INK, 3)
    f.text(xj, 330, "barrier: join", 13, color=INK, bold=True)
    f.line(xf, 90, xf, 310, GRAY, 1.5, dash="3 3")
    f.text(xf, 330, "fork", 13, color=INK, bold=True)
    f.text((x0 + xf) / 2, 365, "serial", 14, color=GRAY)
    f.text((xf + xj) / 2, 365, "parallel region: every thread runs it", 14, color=GRAY)
    f.text((xj + x1) / 2, 365, "serial", 14, color=GRAY)
    f.save("openmp_fork_join.svg")


def schedule_figs():
    # 16 iterations, 4 threads; threads 0-2 on fast cores (1 unit per iteration),
    # thread 3 on a slow core (2 units per iteration).
    speed = [1, 1, 1, 2]
    N = 16

    def timeline(f, top, assign, end_note):
        # assign: list per thread of (start, iters) chunks
        unit = 88
        xL = 190
        rowh, gap = 34, 14
        finish = []
        for k in range(4):
            y = top + k * (rowh + gap)
            f.text(xL - 12, y + rowh / 2 + 5, f"thread {k}" + (", slow core" if speed[k] == 2 else ""), 13, "end",
                   THREAD[k], bold=True)
            t_end = 0
            for (t0, its) in assign[k]:
                w = len(its) * speed[k] * unit
                f.rect(xL + t0 * unit, y, w, rowh, TINT[THREAD[k]], THREAD[k], 1.5, 3)
                f.text(xL + t0 * unit + w / 2, y + rowh / 2 + 5, " ".join(str(i) for i in its), 13, color=INK)
                t_end = max(t_end, t0 + len(its) * speed[k])
            finish.append(t_end)
        tmax = max(finish)
        for k in range(4):
            y = top + k * (rowh + gap)
            if finish[k] < tmax:
                f.line(xL + finish[k] * unit + 4, y + rowh / 2, xL + tmax * unit - 4, y + rowh / 2, GRAY, 1.5, dash="5 4")
                f.text(xL + (finish[k] + tmax) / 2 * unit, y + rowh / 2 - 6, "waits", 12, color=GRAY, italic=True)
        yb0, yb1 = top - 10, top + 4 * (rowh + gap) - gap + 10
        f.line(xL + tmax * unit, yb0, xL + tmax * unit, yb1, INK, 3)
        f.text(xL + tmax * unit + 8, yb1 - 2, end_note, 13, "start", INK, bold=True)
        # time axis
        ya = yb1 + 22
        f.line(xL, ya, xL + 8 * unit + 10, ya, GRAY, 1.2, arrow=True)
        for t in range(9):
            f.line(xL + t * unit, ya - 4, xL + t * unit, ya + 4, GRAY, 1.2)
            f.text(xL + t * unit, ya + 19, t, 12, color=GRAY)
        f.text(xL + 8 * unit + 30, ya + 5, "time", 12, "start", GRAY)
        return tmax

    # static
    f = Fig(1000, 340, "schedule(static): the split is decided before the loop starts",
            "Sixteen iterations divided into four contiguous blocks of four, one per thread. Threads 0 to 2 are on fast cores "
            "and finish at time 4. Thread 3 is on a slow core, takes twice as long per iteration, and finishes at time 8. "
            "The other threads wait at the barrier, so the loop takes 8.")
    assign = [[(0, list(range(4 * k, 4 * k + 4)))] for k in range(4)]
    t = timeline(f, 80, assign, "loop ends at 8")
    f.save("openmp_static.svg")

    # dynamic, chunk 2: simulate a shared counter
    nxt, free_at = 0, [0, 0, 0, 0]
    chunks = [[] for _ in range(4)]
    while nxt < N:
        k = min(range(4), key=lambda j: (free_at[j], j))
        its = list(range(nxt, min(nxt + 2, N)))
        chunks[k].append((free_at[k], its))
        free_at[k] += len(its) * speed[k]
        nxt += len(its)
    f = Fig(1000, 340, "schedule(dynamic, 2): threads take the next chunk when they finish one",
            "The same sixteen iterations handed out two at a time from a shared counter. Each thread takes the next chunk as soon "
            "as it finishes one, so the fast threads take more chunks and the slow thread takes only one. The loop takes 6 "
            "instead of 8.")
    t = timeline(f, 80, chunks, f"loop ends at {max(free_at)}")
    f.save("openmp_dynamic.svg")
    return chunks


# ---------------------------------------------------------------- Lecture 11
def fib_dag():
    f = Fig(1000, 470, "The DAG of fib(4)",
            "A tree of calls. fib(4) spawns fib(3) and continues with fib(2); fib(3) spawns fib(2) and continues with fib(1); "
            "each fib(2) spawns fib(1) and continues with fib(0). Spawn edges are blue and continuation edges are orange. "
            "Calls with no path between them may run at the same time.")
    nodes = {}
    # (name, x, y)
    lay = [("4", 500, 90), ("3", 300, 190), ("2a", 700, 190), ("2b", 180, 290), ("1a", 420, 290),
           ("1b", 600, 290), ("0a", 800, 290), ("1c", 100, 390), ("0b", 260, 390)]
    label = {"4": "fib(4)", "3": "fib(3)", "2a": "fib(2)", "2b": "fib(2)", "1a": "fib(1)", "1b": "fib(1)",
             "0a": "fib(0)", "1c": "fib(1)", "0b": "fib(0)"}
    for n, x, y in lay:
        nodes[n] = (x, y)
    edges = [("4", "3", "s"), ("4", "2a", "c"), ("3", "2b", "s"), ("3", "1a", "c"), ("2a", "1b", "s"),
             ("2a", "0a", "c"), ("2b", "1c", "s"), ("2b", "0b", "c")]
    for a, b, kind in edges:
        (x1, y1), (x2, y2) = nodes[a], nodes[b]
        c = BLUE if kind == "s" else ORANGE
        f.line(x1, y1 + 20, x2, y2 - 22, c, 2.5, arrow=True)
    for n, (x, y) in nodes.items():
        leaf = n[0] in "01"
        f.rect(x - 50, y - 20, 100, 40, "#fff" if leaf else "#f7f7f7", GRAY, 1.5, 8, dash="4 3" if leaf else None)
        f.text(x, y + 5, label[n], 15, bold=not leaf)
    # legend
    f.line(700, 380, 750, 380, BLUE, 2.5, arrow=True)
    f.text(760, 385, "cilk_spawn fib(n-1)", 13, "start", BLUE, bold=True)
    f.line(700, 412, 750, 412, ORANGE, 2.5, arrow=True)
    f.text(760, 417, "continuation: fib(n-2)", 13, "start", ORANGE, bold=True)
    f.rect(700, 430, 50, 26, "#fff", GRAY, 1.5, 6, dash="4 3")
    f.text(760, 448, "base case, returns at once", 13, "start", GRAY)
    f.save("cilk_fib_dag.svg")


def work_stealing():
    f = Fig(1000, 475, "Work stealing: four workers, four deques",
            "Each worker owns a deque of continuations. Workers 0, 1 and 2 push and pop at the bottom of their own deques. "
            "Worker 3's deque is empty, so it steals the oldest continuation, at the top of worker 2's deque.")
    xs = [140, 360, 580, 800]
    depth = [3, 2, 3, 0]
    top_y, box_h = 140, 44
    for k, x in enumerate(xs):
        c = THREAD[k]
        f.text(x, 72, f"worker {k}", 16, color=c, bold=True)
        f.rect(x - 75, top_y - 8, 150, 3 * box_h + 16, "#fafafa", GRAY, 1.3, 6)
        for j in range(depth[k]):
            y = top_y + j * box_h
            lab = "oldest" if j == 0 else ("newest" if j == depth[k] - 1 else "")
            f.rect(x - 65, y, 130, box_h - 6, TINT[c], c, 1.5, 4)
            f.text(x, y + 24, "continuation" if not lab else f"continuation ({lab})", 12)
        if depth[k] == 0:
            f.text(x, top_y + 70, "empty", 14, color=GRAY, italic=True)
        f.text(x, top_y + 3 * box_h + 68, "running" if depth[k] else "idle → thief", 13, color=c if depth[k] else GRAY,
               bold=True)
        if depth[k]:
            yb = top_y + depth[k] * box_h - 6
            f.path(f"M {x-30} {top_y + 3*box_h + 46} L {x-30} {yb + 4}", c, 1.8, arrow=True)
            f.path(f"M {x+30} {yb + 4} L {x+30} {top_y + 3*box_h + 46}", c, 1.8, arrow=True)
    # steal arrow from W2 top to W3
    f.path(f"M {xs[2]+66} {top_y + 19} C {xs[2]+140} {top_y - 30}, {xs[3]-60} {top_y - 30}, {xs[3]-10} {top_y + 50}",
           INK, 2.5, arrow=True, dash="7 4")
    f.text((xs[2] + xs[3]) / 2, top_y - 36, "steal the oldest, from the top", 13, color=INK, bold=True)
    f.text(60, top_y + 10, "top", 13, "end", GRAY)
    f.text(60, top_y + 3 * box_h - 10, "bottom", 13, "end", GRAY)
    f.text(500, 385, "push a continuation at each cilk_spawn; pop it back when the spawned call returns",
           13, color=INK)
    f.text(500, 425, "The owner works at the bottom, newest first. A thief takes from the top, oldest first:", 14, color=INK)
    f.text(500, 447, "usually the biggest piece of unstarted work, so one steal keeps it busy for a long time.", 14,
           color=INK)
    f.save("cilk_work_stealing.svg")


# ---------------------------------------------------------------- Lecture 12
def prefix_chain():
    f = Fig(1000, 250, "The serial prefix sum is a chain",
            "out[0] is in[0]. Each out[i] is computed from out[i-1] and in[i], so every addition waits for the one before it. "
            "The work and the span are both N, and the parallelism is 1.")
    n = 6
    xs = [90 + i * 155 for i in range(n)]
    yo, yi = 160, 80
    for i, x in enumerate(xs):
        lab_o = f"out[{i}]" if i < n - 1 else "out[N-1]"
        lab_i = f"in[{i}]" if i < n - 1 else "in[N-1]"
        if i == n - 1:
            f.text((xs[i - 1] + x) / 2, yo - 10, "…", 22, color=GRAY)
        f.rect(x - 45, yi - 18, 90, 36, TINT[GREEN], GREEN, 1.5, 6)
        f.text(x, yi + 5, lab_i, 14)
        f.rect(x - 45, yo - 18, 90, 36, TINT[BLUE], BLUE, 1.5, 6)
        f.text(x, yo + 5, lab_o, 14, bold=True)
        f.line(x, yi + 18, x, yo - 20, GREEN, 1.8, arrow=True)
        if 0 < i < n - 1:
            f.line(xs[i - 1] + 45, yo, x - 47, yo, BLUE, 2.5, arrow=True)
    f.line(xs[n - 2] + 45, yo, xs[n - 2] + 60, yo, BLUE, 2.5)
    f.line(xs[n - 1] - 62, yo, xs[n - 1] - 47, yo, BLUE, 2.5, arrow=True)
    f.text(500, 222, "out[i] = out[i-1] + in[i]: each addition waits for the previous one", 14, color=INK)
    f.save("prefix_chain.svg")


def prefix_three_pass():
    f = Fig(1000, 560, "Scan, then add: three passes over blocks",
            "Nine elements a through i in three blocks of three, one per thread. Pass 1: each thread scans its own block and "
            "saves its total. Pass 2: one thread scans the three totals into offsets 0, a+b+c, and a+b+c+d+e+f. Pass 3: each "
            "thread adds its block's offset to every element of its block.")
    cw, ch = 82, 36
    xL = 130

    def row(y, vals, label, hl=None):
        f.text(xL - 20, y + ch / 2 + 5, label, 14, "end", INK, bold=True)
        for j, v in enumerate(vals):
            b = j // 3
            x = xL + j * (cw + 4) + b * 18
            c = THREAD[b]
            fill = TINT[c] if (hl is None or b in hl) else "#fff"
            f.rect(x, y, cw, ch, fill, c, 1.5, 4)
            f.text(x + cw / 2, y + ch / 2 + 5, v, 13)

    def bx(b):
        return xL + (3 * b + 1) * (cw + 4) + b * 18 + cw / 2

    row(70, list("abcdefghi"), "in")
    for b in range(3):
        f.text(bx(b), 128, f"thread {b}", 13, color=THREAD[b], bold=True)
    row(140, ["a", "a+b", "a+b+c", "d", "d+e", "d+e+f", "g", "g+h", "g+h+i"], "pass 1")
    # totals
    ty = 230
    tots = ["a+b+c", "d+e+f", "g+h+i"]
    for b in range(3):
        f.line(bx(b) + cw, 178, bx(b) + cw, ty - 2, THREAD[b], 1.6, arrow=True) if False else None
    f.text(xL - 20, ty + 23, "pass 2", 14, "end", INK, bold=True)
    for b, t in enumerate(tots):
        x = xL + b * 130
        f.rect(x, ty, 120, ch, TINT[THREAD[b]], THREAD[b], 1.5, 4)
        f.text(x + 60, ty + 23, t, 13)
    f.text(xL + 3 * 130 + 20, ty + 15, "one thread scans the block totals", 12, "start", GRAY)
    f.text(xL + 3 * 130 + 20, ty + 33, "into offsets: each block's offset is the sum", 12, "start", GRAY)
    f.text(xL + 3 * 130 + 20, ty + 51, "of every block before it", 12, "start", GRAY)
    oy = 300
    offs = ["0", "a+b+c", "a+…+f"]
    for b, o in enumerate(offs):
        x = xL + b * 130
        f.rect(x, oy, 120, ch, "#fff", THREAD[b], 1.5, 4, dash="4 3")
        f.text(x + 60, oy + 23, f"offset {o}", 13)
        if b:   # offset b = offset b-1 + total b-1: a scan of the totals
            f.line(xL + (b - 1) * 130 + 60, ty + ch + 2, x + 40, oy - 2, GRAY, 1.3, arrow=True)
            f.line(xL + (b - 1) * 130 + 120, oy + ch / 2, x - 2, oy + ch / 2, GRAY, 1.3, arrow=True)
    row(400, ["a", "a+b", "a+b+c", "a+…+d", "a+…+e", "a+…+f", "a+…+g", "a+…+h", "a+…+i"], "pass 3", hl={1, 2})
    for b in (1, 2):
        f.line(xL + b * 130 + 60, oy + ch + 2, bx(b), 396, THREAD[b], 1.8, arrow=True)
    f.text(500, 490, "Every block's pass 1 and pass 3 run in parallel; pass 2 is only one number per block.", 14,
           color=INK)
    f.text(500, 515, "About 2N additions in all, against the serial N − 1.", 14, color=INK)
    f.save("prefix_three_pass.svg")


def hillis_steele():
    f = Fig(1000, 470, "Hillis–Steele scan: log₂ N steps, every element busy in each",
            "Eight elements x0 to x7. In step 0 every element adds the element one to its left; in step 1, the element two to "
            "its left; in step 2, the element four to its left. After three steps each element holds the sum of itself and "
            "everything before it.")
    n = 8
    cw = 96
    xL = 130
    xs = [xL + i * (cw + 8) for i in range(n)]
    rows = [["x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"],
            ["0", "0–1", "1–2", "2–3", "3–4", "4–5", "5–6", "6–7"],
            ["0", "0–1", "0–2", "0–3", "1–4", "2–5", "3–6", "4–7"],
            ["0", "0–1", "0–2", "0–3", "0–4", "0–5", "0–6", "0–7"]]
    ys = [70, 170, 270, 370]
    labels = ["input", "step 0: +x[i−1]", "step 1: +x[i−2]", "step 2: +x[i−4]"]
    cols = [GRAY, BLUE, ORANGE, GREEN]
    for r, (y, vals) in enumerate(zip(ys, rows)):
        f.text(xL - 14, y + 23, labels[r], 13, "end", cols[r], bold=True)
        for i, v in enumerate(vals):
            f.rect(xs[i], y, cw, 36, TINT[cols[r]] if r else "#f7f7f7", cols[r], 1.5, 4)
            f.text(xs[i] + cw / 2, y + 23, v if r == 0 else f"sum {v}", 13)
    for r in (1, 2, 3):
        d = 2 ** (r - 1)
        y0, y1 = ys[r - 1] + 36, ys[r]
        for i in range(n):
            f.line(xs[i] + cw / 2, y0 + 2, xs[i] + cw / 2, y1 - 3, GRAY, 1.0, arrow=True)
            if i >= d:
                f.line(xs[i - d] + cw / 2 + 6, y0 + 2, xs[i] + cw / 2 - 6, y1 - 3, cols[r], 1.6, arrow=True)
    f.text(500, 440, "Span: log₂ N steps.   Work: about N additions in every step, N log₂ N in all.", 14, color=INK)
    f.save("prefix_hillis_steele.svg")


fork_join()
schedule_figs()
fib_dag()
work_stealing()
prefix_chain()
prefix_three_pass()
hillis_steele()
