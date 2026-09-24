#!/usr/bin/env python3
"""Generate 1_core_cache.html from the .csv files in this directory.

The other three pages here were hand-authored, which meant that re-measuring
turned into an afternoon of editing SVG coordinates by hand.  This one is
generated: run `make page` after `make data` and every figure, table and
headline number follows the data.

Writes a body fragment to stdout, or the standalone page with --standalone.
"""
import csv, math, sys
from pathlib import Path

HERE = Path(__file__).parent
R = lambda n: list(csv.DictReader(open(HERE / n)))

lat    = [r for r in R("latency.csv") if r["mode"] == "random"]
seq    = {int(r["bytes"]): float(r["ns"]) for r in R("latency.csv") if r["mode"] == "seq"}
rowcol = R("row_column.csv")
_sb    = [r for r in csv.reader(open(HERE / "stride_bandwidth.csv")) if r]
stride = [r for r in _sb if r[0] == "stride"]
_vw    = [r for r in csv.reader(open(HERE / "vector_width.csv")) if r]
width  = [r for r in _vw if r[0] == "width"]
align  = [r for r in _vw if r[0] == "align"]

def at(target):
    return min(lat, key=lambda r: abs(math.log(int(r["bytes"]) / target)))

L1, L2, L3, DRAM = (at(t) for t in (32 << 10, 256 << 10, 8 << 20, 128 << 20))
W16, W1M, W8M, WDR = width[0], width[6], width[9], width[-1]
sz = lambda b: f"{b>>10} KB" if b < (1 << 20) else f"{b>>20} MB"

# ---------------------------------------------------------------- ladder
def fig_ladder():
    """The same idea as cache_map.html's latency ladder, but stopping at DRAM:
    a loop never reaches the storage tier, and this page is scoped to one core."""
    ghz = float(L1["cycles"]) / float(L1["ns"])          # cycles per ns, from the data
    tiers = [("register", 1.0 / ghz, "sv-dim",  "st-dim",  1),
             ("L1d",      float(L1["ns"]),   "sv-z5",   "st-z5",   0),
             ("L2",       float(L2["ns"]),   "sv-z5",   "st-z5",   1),
             ("L3",       float(L3["ns"]),   "sv-z5",   "st-z5",   0),
             ("DRAM",     float(DRAM["ns"]), "sv-cost", "st-cost", 1)]
    lo, hi = math.log10(0.1), math.log10(1000)
    X = lambda v: 80 + (math.log10(v) - lo) * (860 / (hi - lo))
    AX = 150
    g = [f'<line x1="80" y1="{AX}" x2="950" y2="{AX}" class="st-rule" stroke-width="1.25"/>']
    for d in (0.1, 1, 10, 100, 1000):
        lab = f"{d:g} ns" if d < 1000 else "1 us"
        g += [f'<line x1="{X(d):.0f}" y1="{AX-4}" x2="{X(d):.0f}" y2="{AX+4}" class="st-rule" stroke-width="1"/>',
              f'<text x="{X(d):.0f}" y="{AX+20}" text-anchor="middle" class="mono-sm sv-faint">{lab}</text>']
    l1ns = float(L1["ns"])
    for name, v, sf, sc, up in tiers:
        x = X(v)
        top = 60 if up else 96
        g += [f'<line x1="{x:.0f}" y1="{AX}" x2="{x:.0f}" y2="{top+14}" class="{sc}" stroke-width="1.5"/>',
              f'<circle cx="{x:.0f}" cy="{AX}" r="4.5" class="{sf}"/>',
              f'<text x="{x:.0f}" y="{top+6}" text-anchor="middle" class="lbl sv-ink w600">{name}</text>',
              f'<text x="{x:.0f}" y="{top-6}" text-anchor="middle" class="mono-sm {sf}">'
              f'{v:.2f} ns</text>' if v < 10 else
              f'<text x="{x:.0f}" y="{top-6}" text-anchor="middle" class="mono-sm {sf}">{v:.0f} ns</text>']
        # human scale: rescale so an L1 hit takes one second
        r = v / l1ns
        human = (f"{r:.2f} s" if r < 10 else f"{r:.0f} s" if r < 60 else f"{r/60:.1f} min")
        g.append(f'<text x="{x:.0f}" y="{AX+60}" text-anchor="middle" class="mono {sf} w500">{human}</text>')
    g += [f'<line x1="80" y1="{AX+38}" x2="950" y2="{AX+38}" class="st-grid" stroke-width="1" stroke-dasharray="3 4"/>',
          f'<text x="80" y="{AX+32}" class="cap sv-faint">if an L1 hit took one second</text>',
          f'<text x="80" y="30" class="cap sv-faint">actual latency, log scale</text>',
          f'<text x="950" y="{AX+82}" text-anchor="end" class="lbl-sm sv-dim">'
          f'a DRAM miss is {float(DRAM["ns"])/l1ns:.0f} L1 hits &mdash; about 500 instructions of waiting</text>']
    return "\n          ".join(g)



# ------------------------------------------------- L1d geometry, from /sys
def _sysl1(f, d):
    try: return Path("/sys/devices/system/cpu/cpu0/cache/index0").joinpath(f).read_text().strip()
    except OSError: return d

L1_SETS  = int(_sysl1("number_of_sets", "64"))
L1_WAYS  = int(_sysl1("ways_of_associativity", "12"))
L1_LINE  = int(_sysl1("coherency_line_size", "64"))
L1_BYTES = L1_SETS * L1_WAYS * L1_LINE
L1_OFF   = L1_LINE.bit_length() - 1        # bits naming the byte within a line
L1_IDX   = L1_SETS.bit_length() - 1        # bits naming the set
L1_WRAP  = L1_SETS * L1_LINE               # the set index repeats every this many bytes


def fig_setsways():
    """The cache as a grid: one row per set, one column per way, one line per
    cell.  Eight sets drawn, then a break, then the last."""
    CW, CH, X0, Y0 = 50, 30, 200, 104
    shown = list(range(8)) + [None, L1_SETS - 1]
    g = [f'<path d="M{X0},92 L{X0},84 L{X0+L1_WAYS*CW},84 L{X0+L1_WAYS*CW},92" class="st-z5 fill-none" stroke-width="1.5"/>',
         f'<text x="{X0+L1_WAYS*CW//2}" y="74" text-anchor="middle" class="lbl sv-z5 w600">{L1_WAYS} ways</text>',
         f'<text x="{X0+L1_WAYS*CW//2}" y="56" text-anchor="middle" class="lbl-sm sv-dim">a line may sit in any one of them</text>']
    for w in range(L1_WAYS):
        g.append(f'<text x="{X0+w*CW+CW//2}" y="{Y0-6}" text-anchor="middle" class="mono-sm sv-faint">{w}</text>')

    y = Y0
    for st in shown:
        if st is None:
            g.append(f'<text x="{X0+L1_WAYS*CW//2}" y="{y+21}" text-anchor="middle" class="sv-faint" style="font-size:19px">&middot; &middot; &middot;</text>')
            y += CH; continue
        g.append(f'<text x="{X0-12}" y="{y+20}" text-anchor="end" class="mono-sm sv-faint">set {st}</text>')
        for w in range(L1_WAYS):
            x, hot = X0 + w*CW, (st == 0 and w == 0)
            g += [f'<rect x="{x}" y="{y}" width="{CW}" height="{CH}" class="{"fill-cost" if hot else "fill-sunken"}"/>',
                  f'<rect x="{x}" y="{y}" width="{CW}" height="{CH}" class="{"st-cost" if hot else "st-grid"}" stroke-width="{1.75 if hot else 0.75}"/>',
                  f'<text x="{x+CW//2}" y="{y+19}" text-anchor="middle" class="mono-sm sv-faint">{L1_LINE}B</text>']
        y += CH
    H = y
    mid = (Y0 + H) // 2
    g += [f'<path d="M{X0-72},{Y0} L{X0-80},{Y0} L{X0-80},{H} L{X0-72},{H}" class="st-z5c fill-none" stroke-width="1.5"/>',
          f'<text x="{X0-92}" y="{mid}" text-anchor="middle" class="lbl sv-z5c w600" transform="rotate(-90 {X0-92} {mid})">{L1_SETS} sets</text>',
          f'<text x="{X0-108}" y="{mid}" text-anchor="middle" class="lbl-sm sv-dim" transform="rotate(-90 {X0-108} {mid})">the address picks exactly one</text>',
          f'<line x1="{X0+CW+6}" y1="{Y0+15}" x2="{X0+L1_WAYS*CW+30}" y2="{Y0+15}" class="st-cost" stroke-width="1" stroke-dasharray="3 3"/>',
          f'<text x="{X0+L1_WAYS*CW+36}" y="{Y0+11}" class="mono-sm sv-cost w600">one cell = one {L1_LINE}-byte line</text>',
          f'<text x="{X0+L1_WAYS*CW+36}" y="{Y0+25}" class="mono-sm sv-faint">plus its tag and valid bit</text>',
          f'<text x="{X0-118}" y="{H+50}" class="lbl sv-ink w600">{L1_SETS} sets &times; {L1_WAYS} ways &times; {L1_LINE} bytes = {L1_BYTES:,} bytes = {L1_BYTES//1024} KB</text>',
          f'<text x="{X0-118}" y="{H+70}" class="lbl-sm sv-dim">exactly what /sys reports for this L1d. The capacity is not a separate fact &mdash; it is the product of the other three.</text>']
    return "\n          ".join(g)


def fig_addrmap():
    """Which set an address lands in: the low bits name the byte, the next
    bits name the set, everything above is the tag."""
    NB, BW, X0, Y0 = 16, 46, 120, 74          # bits shown, cell width
    g = [f'<text x="{X0-70}" y="{Y0+22}" text-anchor="end" class="mono-sm sv-faint">address bit</text>']
    for i in range(NB):
        b = NB - 1 - i
        x = X0 + i*BW
        zone = ("fill-sunken" if b >= L1_OFF + L1_IDX else
                "fill-z5c" if b >= L1_OFF else "fill-z5")
        g += [f'<rect x="{x}" y="{Y0}" width="{BW}" height="34" class="{zone}"/>',
              f'<rect x="{x}" y="{Y0}" width="{BW}" height="34" class="st-grid" stroke-width="0.75"/>',
              f'<text x="{x+BW//2}" y="{Y0+22}" text-anchor="middle" class="mono-sm sv-dim">{b}</text>']
    tagw, idxw, offw = (NB-L1_OFF-L1_IDX)*BW, L1_IDX*BW, L1_OFF*BW
    for x, w, lab, sub, cls in ((X0, tagw, "tag", "is this the line I want?", "sv-faint"),
                                (X0+tagw, idxw, f"set index &mdash; {L1_IDX} bits", f"{L1_SETS} sets", "sv-z5c"),
                                (X0+tagw+idxw, offw, f"byte in line &mdash; {L1_OFF} bits", f"{L1_LINE} bytes", "sv-z5")):
        g += [f'<text x="{x+w//2}" y="{Y0-22}" text-anchor="middle" class="cap {cls}">{lab}</text>',
              f'<text x="{x+w//2}" y="{Y0-8}" text-anchor="middle" class="mono-sm sv-faint">{sub}</text>']

    # the set strip
    SY, SW = 250, 15
    SX = 60                                   # the strip is wider than the bit bar,
                                              # so pin it left rather than centring
    for st in range(L1_SETS):
        x = SX + st*SW
        g += [f'<rect x="{x}" y="{SY}" width="{SW}" height="28" class="fill-sunken"/>',
              f'<rect x="{x}" y="{SY}" width="{SW}" height="28" class="st-grid" stroke-width="0.6"/>']
        if st % 8 == 0 or st == L1_SETS-1:
            g.append(f'<text x="{x+SW//2}" y="{SY+44}" text-anchor="middle" class="mono-sm sv-faint">{st}</text>')
    g.append(f'<text x="{SX}" y="{SY-44}" class="cap sv-z5c">the {L1_SETS} sets</text>')

    # worked examples
    ex = [(0x0000, "0x0000"), (0x0040, "0x0040"), (0x0800, "0x0800"), (L1_WRAP, f"0x{L1_WRAP:04X}")]
    for i, (a, lab) in enumerate(ex):
        st = (a >> L1_OFF) % L1_SETS
        x = SX + st*SW + SW/2
        ly = 168 + (i % 2)*24
        hot = (a == L1_WRAP)
        cls = "sv-cost w600" if hot else "sv-dim"
        stroke = "st-cost" if hot else "st-dim"
        g += [f'<line x1="{x:.0f}" y1="{ly+6}" x2="{x:.0f}" y2="{SY-2}" class="{stroke}" stroke-width="{1.5 if hot else 1}" stroke-dasharray="3 3"/>',
              f'<text x="{x:.0f}" y="{ly}" text-anchor="middle" class="mono-sm {cls}">{lab} &rarr; set {st}</text>']
        if hot:
            g += [f'<rect x="{SX+st*SW}" y="{SY}" width="{SW}" height="28" class="fill-cost"/>',
                  f'<rect x="{SX+st*SW}" y="{SY}" width="{SW}" height="28" class="st-cost" stroke-width="1.5"/>']
    g += [f'<text x="{SX}" y="{SY+74}" class="lbl sv-cost w600">'
          f'The set index is {L1_IDX} bits, so it repeats every {L1_WRAP:,} bytes.</text>',
          f'<text x="{SX}" y="{SY+92}" class="lbl-sm sv-dim">'
          f'0x0000 and 0x{L1_WRAP:04X} are {L1_WRAP:,} bytes apart and land in the same set. So does every '
          f'address {L1_WRAP:,} bytes after that &mdash; which is the whole of the power-of-two trap.</text>']
    return "\n          ".join(g)

# ---------------------------------------------------------------- set map
def fig_sets():
    """The power-of-two trap, cell by cell: each array element labelled with
    the L1 set its cache line lands in.  set = ((r*lda + c) * 8 // 64) % 64."""
    ROWS, COLS, CELL = 8, 16, 32
    PANELS = ((70, 1024, "packed &mdash; lda = 1024", "st-cost", "sv-cost"),
              (700, 1025, "padded &mdash; lda = 1025", "st-z5", "sv-z5"))
    TINT = {0: "fill-cost", 1: "fill-z5", 2: "fill-z5c"}
    g = ['<text x="70" y="20" class="lbl sv-ink w600">Row-major, C order &mdash; consecutive '
         '<tspan class="mono">c</tspan> are contiguous in memory.</text>',
         '<text x="70" y="34" class="lbl-sm sv-dim">'
         '<tspan class="mono">lda</tspan> is the row length in elements, so the padding adds one '
         'element to each row. (In Fortran or BLAS, <tspan class="mono">lda</tspan> would count '
         'rows instead and the picture would be transposed.)</text>']
    for x0, lda, title, accent, ink in PANELS:
        g.append(f'<text x="{x0}" y="62" class="cap {ink}">{title}</text>')
        g.append(f'<text x="{x0}" y="78" class="lbl-sm sv-dim">each cell shows the L1 set its line falls in</text>')
        for c in range(COLS):
            g.append(f'<text x="{x0+c*CELL+CELL//2}" y="106" text-anchor="middle" '
                     f'class="mono-sm sv-faint">{c}</text>')
        for r in range(ROWS):
            y = 120 + r*CELL
            g.append(f'<text x="{x0-9}" y="{y+21}" text-anchor="end" class="mono-sm sv-faint">{r}</text>')
            for c in range(COLS):
                st = ((r*lda + c) * 8 // 64) % 64
                x = x0 + c*CELL
                g += [f'<rect x="{x}" y="{y}" width="{CELL}" height="{CELL}" class="{TINT.get(st,"fill-sunken")}"/>',
                      f'<rect x="{x}" y="{y}" width="{CELL}" height="{CELL}" class="st-grid" stroke-width="0.75"/>',
                      f'<text x="{x+CELL//2}" y="{y+21}" text-anchor="middle" class="mono-sm sv-dim">{st}</text>']
        # the column being walked
        g += [f'<rect x="{x0}" y="120" width="{CELL}" height="{ROWS*CELL}" class="fill-none {accent}" stroke-width="2.5"/>',
              f'<text x="{x0-9}" y="98" text-anchor="end" class="mono-sm {ink} w600">col 0</text>']
        g.append(f'<text x="{x0-30}" y="{120+ROWS*CELL//2}" text-anchor="middle" '
                 f'class="mono-sm sv-faint" transform="rotate(-90 {x0-30} {120+ROWS*CELL//2})">row</text>')

        # walking down column 0 for 24 rows
        yb = 430
        g.append(f'<text x="{x0}" y="{yb-12}" class="lbl sv-ink w600">walking down column 0, rows 0&ndash;23</text>')
        for r in range(24):
            st = ((r*lda) * 8 // 64) % 64
            x = x0 + r*21
            g += [f'<rect x="{x}" y="{yb}" width="21" height="26" class="{TINT.get(st,"fill-sunken")}"/>',
                  f'<rect x="{x}" y="{yb}" width="21" height="26" class="st-grid" stroke-width="0.75"/>',
                  f'<text x="{x+10}" y="{yb+18}" text-anchor="middle" class="mono-sm sv-dim">{st}</text>']
        reached = len({((r*lda)*8//64) % 64 for r in range(1024)})
        msg = (f"all 1024 elements land in set 0 &mdash; 12 ways to hold them"
               if reached == 1 else
               f"the set advances every 8 rows &mdash; the column reaches all {reached} sets")
        g.append(f'<text x="{x0}" y="{yb+48}" class="lbl {ink} w600">{msg}</text>')
    return "\n          ".join(g)

# ---------------------------------------------------------------- figure 1
def fig_latency():
    lo, hi = math.log2(8192), math.log2(256 << 20)
    X = lambda b: 110 + (math.log2(b) - lo) * (850 / (hi - lo))
    ylo, yhi = math.log10(0.6), math.log10(130)
    Y = lambda v: 250 - (math.log10(v) - ylo) * (200 / (yhi - ylo))
    pts = " ".join(f"{X(int(r['bytes'])):.0f},{Y(float(r['ns'])):.0f}" for r in lat)
    seqpts = " ".join(f"{X(b):.0f},{Y(v):.0f}" for b, v in sorted(seq.items()))
    g = [f'<polyline class="st-cost fill-none" stroke-width="2.5" points="{pts}"/>',
         f'<polyline class="st-z5c fill-none" stroke-width="2" stroke-dasharray="5 4" points="{seqpts}"/>']
    for kb, n in ((48, "L1d 48 KB"), (1024, "L2 1 MB"), (16384, "L3 16 MB")):
        x = X(kb * 1024)
        g += [f'<line x1="{x:.0f}" y1="40" x2="{x:.0f}" y2="250" class="st-grid" stroke-width="1" stroke-dasharray="3 4"/>',
              f'<text x="{x+6:.0f}" y="54" class="mono-sm sv-faint">{n}</text>']
    for v in (1, 2, 5, 10, 20, 50, 100):
        g += [f'<text x="{100:.0f}" y="{Y(v)+4:.0f}" text-anchor="end" class="mono-sm sv-faint">{v}</text>']
    for b in (8192, 65536, 524288, 4 << 20, 32 << 20, 256 << 20):
        g += [f'<text x="{X(b):.0f}" y="272" text-anchor="middle" class="mono-sm sv-faint">{sz(b)}</text>']
    g += [f'<text x="{X(256<<20)-10:.0f}" y="{Y(float(DRAM["ns"]))-12:.0f}" text-anchor="end" class="mono sv-cost w600">one access at a time</text>',
          f'<text x="{X(256<<20)-10:.0f}" y="{Y(seq[max(seq)])-10:.0f}" text-anchor="end" class="mono sv-z5c w600">sequential &mdash; prefetched</text>',
          '<line x1="110" y1="250" x2="960" y2="250" class="st-rule" stroke-width="1.25"/>',
          '<line x1="110" y1="40" x2="110" y2="250" class="st-rule" stroke-width="1.25"/>',
          '<text x="535" y="292" text-anchor="middle" class="lbl sv-dim">working set</text>',
          '<text x="70" y="30" class="cap sv-faint">ns per access</text>']
    return "\n          ".join(g)

# ---------------------------------------------------------------- figure 2
def fig_stride():
    X = lambda s: 120 + (math.log2(s) - 3) * (830 / 10)
    ylo, yhi = math.log10(0.5), math.log10(50)
    Y = lambda v: 250 - (math.log10(v) - ylo) * (200 / (yhi - ylo))
    u = " ".join(f"{X(int(r[1])):.0f},{Y(float(r[3])):.0f}" for r in stride)
    f = " ".join(f"{X(int(r[1])):.0f},{Y(float(r[4])):.0f}" for r in stride)
    g = [f'<polyline class="st-z5 fill-none" stroke-width="2.5" points="{f}"/>',
         f'<polyline class="st-cost fill-none" stroke-width="2.5" points="{u}"/>',
         f'<line x1="{X(64):.0f}" y1="40" x2="{X(64):.0f}" y2="250" class="st-grid" stroke-width="1.2" stroke-dasharray="3 4"/>',
         f'<text x="{X(64)+7:.0f}" y="54" class="mono-sm sv-faint">64 B = one line</text>']
    for v in (1, 2, 5, 10, 20, 40):
        g.append(f'<text x="110" y="{Y(v)+4:.0f}" text-anchor="end" class="mono-sm sv-faint">{v}</text>')
    for s in (8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192):
        g.append(f'<text x="{X(s):.0f}" y="272" text-anchor="middle" class="mono-sm sv-faint">{s}</text>')
    g += [f'<text x="{X(8192)-8:.0f}" y="{Y(float(stride[-1][4]))-10:.0f}" text-anchor="end" class="mono sv-z5 w600">moved by the hardware</text>',
          f'<text x="{X(8192)-8:.0f}" y="{Y(float(stride[-1][3]))+18:.0f}" text-anchor="end" class="mono sv-cost w600">used by the loop</text>',
          '<line x1="120" y1="250" x2="950" y2="250" class="st-rule" stroke-width="1.25"/>',
          '<line x1="120" y1="40" x2="120" y2="250" class="st-rule" stroke-width="1.25"/>',
          '<text x="535" y="292" text-anchor="middle" class="lbl sv-dim">stride in bytes</text>',
          '<text x="70" y="30" class="cap sv-faint">GB/s</text>']
    return "\n          ".join(g)

# ---------------------------------------------------------------- figure 3
def fig_width():
    X  = lambda b: 100 + (math.log2(b) - 14) * (430 / 14)
    Y  = lambda v: 230 - (math.log10(v) - math.log10(30)) * (180 / (math.log10(340) - math.log10(30)))
    X2 = lambda b: X(b) + 600
    Y2 = lambda v: 230 - (v - 1) * (190 / 5)
    series = [("scalar", 2, "st-dim", "sv-dim"), ("256-bit", 3, "st-z5c", "sv-z5c"),
              ("512-bit", 4, "st-cost", "sv-cost")]
    g = []
    for name, idx, sc, sf in series:
        pts = " ".join(f"{X(int(r[1])):.0f},{Y(float(r[idx])):.0f}" for r in width)
        g.append(f'<polyline class="{sc} fill-none" stroke-width="2.5" points="{pts}"/>')
        g.append(f'<text x="{X(int(width[0][1]))+6:.0f}" y="{Y(float(width[0][idx]))-8:.0f}" class="mono-sm {sf} w600">{name}</text>')
    for name, idx, sc, sf in series[1:]:
        pts = " ".join(f"{X2(int(r[1])):.0f},{Y2(float(r[idx])/float(r[2])):.0f}" for r in width)
        g.append(f'<polyline class="{sc} fill-none" stroke-width="2.5" points="{pts}"/>')
    for kb, n in ((48, "L1d"), (1024, "L2"), (16384, "L3")):
        for xf in (X, X2):
            x = xf(kb * 1024)
            g.append(f'<line x1="{x:.0f}" y1="36" x2="{x:.0f}" y2="230" class="st-grid" stroke-width="1" stroke-dasharray="3 4"/>')
            g.append(f'<text x="{x+5:.0f}" y="48" class="mono-sm sv-faint">{n}</text>')
    for v in (40, 80, 160, 320):
        g.append(f'<text x="90" y="{Y(v)+4:.0f}" text-anchor="end" class="mono-sm sv-faint">{v}</text>')
    for v in (1, 2, 3, 4, 5):
        g.append(f'<text x="{X2(16384)-10:.0f}" y="{Y2(v)+4:.0f}" text-anchor="end" class="mono-sm sv-faint">{v}&times;</text>')
    g.append(f'<line x1="{X2(16384):.0f}" y1="{Y2(1):.0f}" x2="{X2(256<<20):.0f}" y2="{Y2(1):.0f}" class="st-rule" stroke-width="1"/>')
    for b in (16384, 262144, 4 << 20, 64 << 20, 256 << 20):
        for xf in (X, X2):
            g.append(f'<text x="{xf(b):.0f}" y="252" text-anchor="middle" class="mono-sm sv-faint">{sz(b)}</text>')
    for x0, x1, lab, y in ((100, 530, "GB/s by working set", 30), (700, 1130, "speedup over scalar", 30)):
        g.append(f'<text x="{x0-30}" y="{y}" class="cap sv-faint">{lab}</text>')
    g += ['<line x1="100" y1="230" x2="530" y2="230" class="st-rule" stroke-width="1.25"/>',
          '<line x1="100" y1="36" x2="100" y2="230" class="st-rule" stroke-width="1.25"/>',
          '<line x1="700" y1="230" x2="1130" y2="230" class="st-rule" stroke-width="1.25"/>',
          '<line x1="700" y1="36" x2="700" y2="230" class="st-rule" stroke-width="1.25"/>',
          f'<text x="{X2(256<<20)-8:.0f}" y="{Y2(1)-10:.0f}" text-anchor="end" class="mono-sm sv-cost w600">1.0&times; &mdash; width stops mattering</text>']
    return "\n          ".join(g)

# ---------------------------------------------------------------- figure 4
def fig_align():
    n = len(align)
    bw, gap = 74, 26
    vmax = max(float(r[2]) for r in align)
    g = []
    for i, r in enumerate(align):
        off, v = int(r[1]), float(r[2])
        x = 120 + i * (bw + gap)
        h = (v / vmax) * 175
        cls = "sv-cost" if off == 0 else "sv-dim"
        g += [f'<rect x="{x}" y="{215-h:.0f}" width="{bw}" height="{h:.0f}" class="{cls}"/>',
              f'<text x="{x+bw//2}" y="{215-h-8:.0f}" text-anchor="middle" class="mono-sm {"sv-cost w600" if off==0 else "sv-dim"}">{v:.0f}</text>',
              f'<text x="{x+bw//2}" y="234" text-anchor="middle" class="mono-sm sv-faint">{off}</text>']
    y0 = 215 - (float(align[0][2]) / vmax) * 175
    g += [f'<line x1="120" y1="{y0:.0f}" x2="920" y2="{y0:.0f}" class="st-cost" stroke-width="1" stroke-dasharray="4 4"/>',
          '<line x1="110" y1="215" x2="930" y2="215" class="st-rule" stroke-width="1.25"/>',
          '<text x="520" y="256" text-anchor="middle" class="lbl sv-dim">starting offset within the line (bytes)</text>',
          '<text x="110" y="28" class="cap sv-faint">GB/s, L1-resident, 512-bit loads</text>']
    return "\n          ".join(g)

FIGS = dict(grid=fig_setsways(), addr=fig_addrmap(), sets=fig_sets(), ladder=fig_ladder(), latency=fig_latency(), stride=fig_stride(), width=fig_width(), align=fig_align())

VALS = dict(
    l1_ns=float(L1["ns"]),   l1_cyc=float(L1["cycles"]),
    l2_ns=float(L2["ns"]),   l2_cyc=float(L2["cycles"]),
    l3_ns=float(L3["ns"]),   l3_cyc=float(L3["cycles"]),
    dram_ns=float(DRAM["ns"]), dram_cyc=float(DRAM["cycles"]),
    xl1=float(DRAM["ns"]) / float(L1["ns"]),
    s8=float(stride[0][3]), s64=float(stride[3][3]), fetched64=float(stride[3][4]),
    w16=[float(W16[i]) for i in (2,3,4)], w1m=[float(W1M[i]) for i in (2,3,4)],
    w8m=[float(W8M[i]) for i in (2,3,4)], wdr=[float(WDR[i]) for i in (2,3,4)],
    al0=float(align[0][2]), aln=sum(float(r[2]) for r in align[1:])/(len(align)-1),
    pack1024=float(next(r["col_ns"] for r in rowcol if r["n"]=="1024" and r["lda"]=="1024")),
    pad1024=float(next(r["col_ns"] for r in rowcol if r["n"]=="1024" and r["lda"]=="1025")),
    row1024=float(next(r["row_ns"] for r in rowcol if r["n"]=="1024" and r["lda"]=="1024")),
)
VALS["ratio16"] = VALS["w16"][2] / VALS["w16"][0]
VALS["ratiodr"] = VALS["wdr"][2] / VALS["wdr"][0]
VALS["alratio"] = VALS["al0"] / VALS["aln"]
VALS["padwin"]  = VALS["pack1024"] / VALS["pad1024"]
def _human(v):
    r = v / VALS["l1_ns"]
    return f"{r:.1f} seconds" if r < 60 else f"{r/60:.1f} minutes"
VALS["l2_human"]   = _human(VALS["l2_ns"])
VALS["l3_human"]   = _human(VALS["l3_ns"])
VALS["dram_human"] = _human(VALS["dram_ns"])

# ============================ the page ============================
HEAD = '''<title>One Core's View</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Archivo:wght@600;700&family=IBM+Plex+Mono:wght@400;500;600&family=IBM+Plex+Sans:wght@400;500;600&display=swap">

<style>
html{-webkit-text-size-adjust:100%}
figure,blockquote,dl,dd{margin:0}
ul,ol{margin:0}
img,svg{max-width:100%}
:root{
  color-scheme:light;   /* this page is light by design */
  --bg:#EEF1F5; --surface:#FFFFFF; --sunken:#E3E8EE;
  --ink:#141B24; --ink-2:#4E5A6B; --ink-3:#7C889A;
  --rule:#C6D0DB; --grid:#D8E0E8;
  --zen5:#A85C15; --zen5-soft:#F2E3D2;
  --zen5c:#166F69; --zen5c-soft:#D7E9E7;
  --cost:#9E2C4A; --cost-soft:#F3DAE1;
  --shadow:0 1px 2px rgba(20,27,36,.06), 0 8px 24px rgba(20,27,36,.06);
}

*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);
  font-family:"IBM Plex Sans",ui-sans-serif,system-ui,-apple-system,"Segoe UI",sans-serif;
  font-size:16px;line-height:1.6;-webkit-font-smoothing:antialiased}
.wrap{max-width:1180px;margin:0 auto;padding:44px 28px 88px;
      display:flex;flex-direction:column;gap:56px}
.masthead{display:flex;flex-direction:column;gap:18px}
.backlink{font-family:"IBM Plex Mono",ui-monospace,monospace;font-size:12px;
  letter-spacing:.04em;color:var(--ink-3);margin:0}
.backlink a{color:var(--ink-2)}
h1{font-family:Archivo,ui-sans-serif,system-ui,sans-serif;font-weight:700;
  font-size:clamp(2.1rem,5vw,3.3rem);line-height:1.04;letter-spacing:-.025em;
  margin:0;text-wrap:balance}
.standfirst{margin:0;max-width:66ch;font-size:1.075rem;color:var(--ink-2)}
.standfirst b{color:var(--ink);font-weight:600}
.machine{display:flex;flex-wrap:wrap;gap:0 26px;
  border-top:1px solid var(--rule);border-bottom:1px solid var(--rule);
  padding:14px 0;margin-top:6px;font-family:"IBM Plex Mono",ui-monospace,monospace;
  font-size:12.5px;color:var(--ink-2)}
.machine b{color:var(--ink);font-weight:500}
section{display:flex;flex-direction:column;gap:20px}
h2{font-family:Archivo,ui-sans-serif,system-ui,sans-serif;font-weight:600;
  font-size:1.55rem;letter-spacing:-.015em;margin:0;text-wrap:balance}
h2 .num{font-family:"IBM Plex Mono",ui-monospace,monospace;font-size:.72em;
  color:var(--ink-3);font-weight:400;margin-right:.7em}
p{margin:0;max-width:70ch}
p + p{margin-top:.9em}
strong{font-weight:600}
.lede{color:var(--ink-2)}
code{font-family:"IBM Plex Mono",ui-monospace,monospace;font-size:.9em;
  background:var(--sunken);padding:1px 5px;border-radius:2px;color:var(--ink)}
figure{margin:0;background:var(--surface);border:1px solid var(--rule);border-radius:3px;
  box-shadow:var(--shadow);padding:26px 26px 20px;display:flex;flex-direction:column;gap:16px}
.canvas{overflow-x:auto;overflow-y:hidden}
figure svg{display:block;width:100%;min-width:700px;height:auto}
figcaption{font-size:13.5px;color:var(--ink-2);max-width:80ch;
  border-top:1px solid var(--grid);padding-top:14px}
figcaption b{color:var(--ink);font-weight:600}
.sv-ink{fill:var(--ink)} .sv-dim{fill:var(--ink-2)} .sv-faint{fill:var(--ink-3)}
.sv-z5{fill:var(--zen5)} .sv-z5c{fill:var(--zen5c)} .sv-cost{fill:var(--cost)}
.st-rule{stroke:var(--rule);fill:none} .st-grid{stroke:var(--grid);fill:none}
.st-dim{stroke:var(--ink-2);fill:none} .st-z5{stroke:var(--zen5);fill:none}
.st-z5c{stroke:var(--zen5c);fill:none} .st-cost{stroke:var(--cost);fill:none}
.fill-none{fill:none}
.fill-sunken{fill:var(--sunken)}
.fill-cost{fill:var(--cost-soft)}
.fill-z5{fill:var(--zen5-soft)}
.fill-z5c{fill:var(--zen5c-soft)}
.lbl{font-family:"IBM Plex Sans",sans-serif;font-size:11px}
.lbl-sm{font-family:"IBM Plex Sans",sans-serif;font-size:9.5px}
.mono{font-family:"IBM Plex Mono",ui-monospace,monospace;font-size:10.5px}
.mono-sm{font-family:"IBM Plex Mono",ui-monospace,monospace;font-size:9px}
.cap{font-family:"IBM Plex Mono",ui-monospace,monospace;font-size:10px;
  letter-spacing:.12em;text-transform:uppercase}
.w500{font-weight:500} .w600{font-weight:600}
.note{background:var(--surface);border:1px solid var(--rule);
  border-left:3px solid var(--zen5);border-radius:0 3px 3px 0;
  padding:18px 20px;display:flex;flex-direction:column;gap:8px}
.note h3{font-family:"IBM Plex Sans",sans-serif;font-size:.95rem;font-weight:600;margin:0}
.note p{font-size:14px;color:var(--ink-2)}
.note.warn{border-left-color:var(--cost)}
.tablewrap{overflow-x:auto;border:1px solid var(--rule);border-radius:3px;background:var(--surface)}
table{border-collapse:collapse;width:100%;min-width:600px;font-size:13.5px}
th,td{padding:11px 16px;text-align:right;border-bottom:1px solid var(--grid);white-space:nowrap}
th:first-child,td:first-child{text-align:left;white-space:normal}
thead th{font-family:"IBM Plex Mono",ui-monospace,monospace;font-size:10px;
  letter-spacing:.1em;text-transform:uppercase;color:var(--ink-3);
  font-weight:500;background:var(--sunken)}
tbody td{font-family:"IBM Plex Mono",ui-monospace,monospace;
  font-variant-numeric:tabular-nums;color:var(--ink-2)}
tbody td:first-child{font-family:"IBM Plex Sans",sans-serif;font-weight:600;color:var(--ink)}
tbody tr:last-child td{border-bottom:none}
tbody tr.hi td{background:var(--zen5-soft)}
tbody tr.bad td{background:var(--cost-soft)}
.colophon{border-top:1px solid var(--rule);padding-top:22px;
  display:flex;flex-direction:column;gap:10px;font-size:13px;color:var(--ink-2)}
.colophon h2{font-size:1.05rem}
a{color:var(--ink);text-decoration-color:var(--ink-3);text-underline-offset:3px}
a:hover{text-decoration-color:var(--ink)}
a:focus-visible{outline:2px solid var(--zen5);outline-offset:3px;border-radius:2px}
@media (max-width:640px){.wrap{padding:32px 18px 64px;gap:44px}figure{padding:18px 16px 14px}}
</style>'''

BODY = '''  <header class="masthead">
    <p class="backlink">&larr; the single-core cut of <a href="{cmap}">Strix Point Cache Map</a></p>
    <h1>One Core&rsquo;s View</h1>
    <p class="standfirst">
      Everything here is what <b>one thread on one core</b> can see. No sharing, no coherence,
      no core-to-core transfer, no threads at all. What is left is the part you need to reason
      about a loop and about whether vectorising it will do anything: how much fits, how much
      moves per access, what a miss costs, and when a wider register stops helping.
    </p>
    <div class="machine">
      <span><b>AMD Ryzen AI 9 HX 370</b> &middot; Zen 5, one core</span>
      <span>L1d 48 KB &middot; L2 1 MB &middot; L3 16 MB</span>
      <span>64-byte lines</span>
      <span>AVX-512</span>
    </div>
  </header>

  <section>
    <h2><span class="num">01</span>Four numbers, and what each one is for</h2>
    <p class="lede">
      Read from <code>/sys</code>, not a datasheet. L1 and L2 belong to this core alone. The L3
      is shared with three others, but inside a single-threaded loop it is simply the last stop
      before DRAM, so it appears here for its capacity and nothing else.
    </p>
    <div class="tablewrap">
      <table>
        <thead><tr><th scope="col">Level</th><th scope="col">Size</th><th scope="col">Ways</th>
          <th scope="col">Sets</th><th scope="col">Latency</th><th scope="col">Cycles</th>
          <th scope="col">What it is for</th></tr></thead>
        <tbody>
          <tr class="hi"><td>L1d</td><td>48 KB</td><td>12</td><td>64</td><td>{l1_ns:.2f} ns</td><td>{l1_cyc:.0f}</td><td>the tile you are aiming at</td></tr>
          <tr><td>L2</td><td>1 MB</td><td>16</td><td>1024</td><td>{l2_ns:.2f} ns</td><td>{l2_cyc:.0f}</td><td>a comfortable blocking target</td></tr>
          <tr><td>L3</td><td>16 MB</td><td>16</td><td>16384</td><td>{l3_ns:.1f} ns</td><td>{l3_cyc:.0f}</td><td>last stop before the cliff</td></tr>
          <tr class="bad"><td>DRAM</td><td>&mdash;</td><td>&mdash;</td><td>&mdash;</td><td>{dram_ns:.0f} ns</td><td>{dram_cyc:.0f}</td><td>{xl1:.0f}&times; an L1 hit</td></tr>
        </tbody>
      </table>
    </div>
    <figure>
      <div class="canvas">
        <svg viewBox="0 0 1000 250" role="img" aria-label="The four latencies on a logarithmic scale from 0.1 nanoseconds to 1 microsecond, with a register at one cycle. Rescaled so an L1 hit takes one second, L2 takes {l2_human}, L3 {l3_human} and a DRAM access {dram_human}.">
          {ladder}
        </svg>
      </div>
      <figcaption>
        <b>The table above understates the spacing.</b> On a log axis the four levels are not
        neighbours: the step from L3 to DRAM alone is larger than everything to its left put
        together. The second row rescales the same measurements so an L1 hit takes one second,
        which is the easiest way to keep {xl1:.0f}&times; in your head. Storage sits three further
        decades to the right and is on the <a href="{cmap}">parent page</a>; a loop never reaches
        it directly.
      </figcaption>
    </figure>
    <p class="lede">
      The same geometry drawn as blocks &mdash; cores, split L1, private L2, the two L3s and main
      memory &mdash; is in <a href="{block}">Twelve Cores, Two L3s</a>.
    </p>
    <div class="note">
      <h3>The one arithmetic fact everything else rests on</h3>
      <p>
        A cache line is <strong>64 bytes</strong>: 8 doubles, or 16 floats. A 512-bit vector is
        also <strong>64 bytes</strong>. So on this machine <strong>one AVX-512 load is exactly one
        cache line</strong>, and a 256-bit load is exactly half of one. That coincidence is why
        &sect;05 and &sect;06 come out the way they do.
      </p>
    </div>
  </section>

  <section>
    <h2><span class="num">02</span>How the cache is organised</h2>
    <p class="lede">
      The table above has a Ways column and a Sets column, and they are not decoration: together
      with the line size they <em>are</em> the capacity. Taking L1d as the example, because it is
      the one your inner loop is aiming at.
    </p>
    <figure>
      <div class="canvas">
        <svg viewBox="0 0 1020 500" role="img" aria-label="The L1 data cache drawn as a grid: {l1sets} rows, one per set, and {l1ways} columns, one per way. Each cell holds one {l1line}-byte line. Eight sets are drawn, then a break, then the last. {l1sets} times {l1ways} times {l1line} bytes is {l1bytes} bytes, which is the cache size.">
          {grid}
        </svg>
      </div>
      <figcaption>
        <b>A set is a row; a way is a column; a cell is one line.</b> The address decides which
        <em>row</em> a line goes in, and it has no say at all about which column &mdash; any of the
        {l1ways} will do. That is the whole of what associativity means: how many places a line
        with a given address is allowed to be.
      </figcaption>
    </figure>
    <figure>
      <div class="canvas">
        <svg viewBox="0 0 1060 400" role="img" aria-label="An address split into three fields: the low {l1off} bits name the byte within the line, the next {l1idx} bits name the set, and everything above is the tag. Four example addresses are mapped onto a strip of {l1sets} sets, showing that 0x0000 and 0x1000 land in the same set because the set index repeats every {l1wrap} bytes.">
          {addr}
        </svg>
      </div>
      <figcaption>
        <b>The address is not consulted as a number &mdash; it is cut into three fields.</b> The
        bottom {l1off} bits pick the byte inside the line and never leave it. The next {l1idx} pick
        the set. Everything above is the tag, which is only compared once the set has been chosen.
        Because the set index is {l1idx} bits taken from a fixed position, it <strong>wraps every
        {l1wrap:,} bytes</strong> &mdash; and that period is the reason a stride which is a multiple
        of {l1wrap:,} can only ever reach one set.
      </figcaption>
    </figure>
  </section>

  <section>
    <h2><span class="num">03</span>Capacity: where the working set stops fitting</h2>
    <p class="lede">
      One dependent access at a time, in random order, so nothing can be prefetched and what you
      see is the real cost of reaching each level. The dashed line is the same walk in address
      order, which is what a well-formed loop gets instead.
    </p>
    <figure>
      <div class="canvas">
        <svg viewBox="0 0 1000 300" role="img" aria-label="Latency per access against working-set size. Random access steps from {l1_ns:.2f} nanoseconds under 48 kilobytes to {l2_ns:.2f} at L2, {l3_ns:.0f} at L3 and {dram_ns:.0f} nanoseconds in DRAM. Sequential access stays nearly flat across the whole range.">
          {latency}
        </svg>
      </div>
      <figcaption>
        <b>Read this as a tiling table.</b> Keep an inner working set under 48&nbsp;KB and it runs
        at {l1_cyc:.0f} cycles per access; let it past 16&nbsp;MB and the same loop pays
        {dram_cyc:.0f}. The block size you choose is just whichever row you are aiming at. The gap
        between the two curves is what the prefetcher is worth &mdash; it is the reason
        &ldquo;make the inner loop sequential&rdquo; outranks everything else on the list.
      </figcaption>
    </figure>
  </section>

  <section>
    <h2><span class="num">04</span>The line: memory moves 64 bytes at a time</h2>
    <p class="lede">
      Ask for one value and the hardware moves the whole line containing it. So the question for
      a loop is not how many bytes it needs, but how much of every line it forces the machine to
      move it actually uses.
    </p>
    <figure>
      <div class="canvas">
        <svg viewBox="0 0 1000 300" role="img" aria-label="Useful bandwidth against fetched bandwidth as the stride grows. Between stride 8 and stride 64 the hardware moves a constant {fetched64:.0f} gigabytes per second while the useful fraction falls from {s8:.0f} to {s64:.1f}.">
          {stride}
        </svg>
      </div>
      <figcaption>
        <b>Same work, one eighth of the payload.</b> From stride 8 to stride 64 the hardware moves
        a flat ~{fetched64:.0f}&nbsp;GB/s of lines while the useful fraction falls from
        {s8:.1f} to {s64:.1f}&nbsp;GB/s &mdash; a factor of 8, which is just
        <code>64 / 8</code>: one useful double per line. This is the whole case for
        structure-of-arrays, and for putting the contiguous index innermost.
      </figcaption>
    </figure>
  </section>

  <section>
    <h2><span class="num">05</span>Vector width: when does a wider register help?</h2>
    <p class="lede">
      The same reduction written three ways &mdash; scalar, 256-bit, 512-bit &mdash; over the same
      data. This is the measurement that decides whether vectorising a given loop is worth
      anything at all.
    </p>
    <figure>
      <div class="canvas">
        <svg viewBox="0 0 1180 300" role="img" aria-label="Left: throughput for scalar, 256-bit and 512-bit reductions against working-set size. In L1 the 512-bit version reaches {w16_512:.0f} gigabytes per second against {w16_s:.0f} scalar. Right: the speedup over scalar, which decays from {ratio16:.1f} times to 1.0 as the working set passes L3 into DRAM.">
          {width}
        </svg>
      </div>
      <figcaption>
        <b>{ratio16:.1f}&times; in L1, {ratiodr:.1f}&times; in DRAM.</b> An L1-resident loop is
        limited by how many bytes per cycle the core can load, and a wider register moves more per
        instruction. A DRAM-resident loop is limited by the memory system, which does not care how
        wide your registers are.
      </figcaption>
    </figure>
    <div class="tablewrap">
      <table>
        <thead><tr><th scope="col">Working set</th><th scope="col">Scalar</th>
          <th scope="col">256-bit</th><th scope="col">512-bit</th>
          <th scope="col">512 vs scalar</th></tr></thead>
        <tbody>
          <tr class="hi"><td>16 KB &mdash; in L1</td><td>{w16_s:.1f}</td><td>{w16_2:.1f}</td><td>{w16_512:.1f}</td><td>{ratio16:.1f}&times;</td></tr>
          <tr><td>1 MB &mdash; in L2</td><td>{w1m_s:.1f}</td><td>{w1m_2:.1f}</td><td>{w1m_512:.1f}</td><td>{ratio1m:.1f}&times;</td></tr>
          <tr><td>8 MB &mdash; in L3</td><td>{w8m_s:.1f}</td><td>{w8m_2:.1f}</td><td>{w8m_512:.1f}</td><td>{ratio8m:.1f}&times;</td></tr>
          <tr class="bad"><td>256 MB &mdash; DRAM</td><td>{wdr_s:.1f}</td><td>{wdr_2:.1f}</td><td>{wdr_512:.1f}</td><td>{ratiodr:.1f}&times;</td></tr>
        </tbody>
      </table>
    </div>
    <div class="note">
      <h3>Blocking and vectorising are the same project</h3>
      <p>
        Vectorising a memory-bound loop changes nothing &mdash; you can watch the speedup decay to
        1.0 in the right-hand panel. Tiling a loop into L1 is what <em>creates</em> the conditions
        under which the vector units pay off. The order is <strong>access pattern, then blocking,
        then width</strong>; a wider vector cannot help a loop that is waiting for memory.
      </p>
    </div>
  </section>

  <section>
    <h2><span class="num">06</span>Alignment is binary, not gradual</h2>
    <p class="lede">
      64-byte lines and 64-byte vectors mean there is no such thing as nearly aligned. Either
      every vector load sits inside one line, or every single one straddles two.
    </p>
    <figure>
      <div class="canvas">
        <svg viewBox="0 0 1000 270" role="img" aria-label="512-bit load throughput against starting offset within the cache line. Offset zero reaches {al0:.0f} gigabytes per second; every other offset from 8 to 56 sits at about {aln:.0f}.">
          {align}
        </svg>
      </div>
      <figcaption>
        <b>{al0:.0f} GB/s aligned, {aln:.0f} GB/s at every other offset &mdash; {alratio:.2f}&times;.</b>
        The penalty is flat from 8 bytes to 56: an offset of 56 is no better than an offset of 8,
        because both mean every load crosses a boundary. This is why
        <code>posix_memalign(&amp;p, 64, n)</code> and <code>aligned</code> clauses matter, and why
        getting &ldquo;closer&rdquo; to aligned buys nothing.
      </figcaption>
    </figure>
  </section>

  <section>
    <h2><span class="num">07</span>The power-of-two trap</h2>
    <p class="lede">
      The last single-core property worth knowing, because it bites exactly when you start
      tiling. L1 has 64 sets and picks one with address bits 6&ndash;11, so a power-of-two row
      length advances the set index by a multiple of 64 &mdash; which is to say not at all.
    </p>
    <figure>
      <div class="canvas">
        <svg viewBox="0 0 1240 500" role="img" aria-label="Two grids of array elements, each cell labelled with the L1 set its cache line falls in. With a packed leading dimension of 1024 the set depends only on the column, so every row is identical and a whole column sits in set 0. With 1025 the boundary shears one place left per row, so walking down a column steps to the next set every eight rows and reaches all 64.">
          {sets}
        </svg>
      </div>
      <figcaption>
        <b>The same array, the same loop, one extra element per row.</b> Row-major throughout:
        the set is <code>((r&middot;lda + c) &middot; 8 / 64) mod 64</code>, so consecutive
        <code>c</code> share a line and <code>lda</code> is the row length. On the left the row term vanishes
        &mdash; 1024 doubles is 128 lines and 128 mod 64 is 0 &mdash; so the set depends on the
        column alone and every row is identical. On the right the extra element shears the pattern
        one place to the left per row, which is the whole fix: the column stops being a single set
        and becomes a walk across all of them.
      </figcaption>
    </figure>
    <p>
      At <code>n = 1024</code> a column traversal costs <strong>{pack1024:.2f} ns</strong> per
      element packed and <strong>{pad1024:.2f} ns</strong> with one extra element per row:
      <strong>{padwin:.0f}&times;</strong> for eight bytes. An entire column competes for one set
      of 12 ways while the other 63 sets sit empty. The rule is not &ldquo;avoid powers of
      two&rdquo; but how many factors of two the row length contains measured in cache lines; a
      row that is an odd number of lines reaches every set. Worked through one bit at a time in
      <a href="{why}">Why 1024 Is Slow</a>.
    </p>
    <p>
      It is worth being precise about what the cache has shrunk <em>to</em>, because the grid in
      section 02 gives the number directly. If every address you touch lands in one set, the only
      lines available to you are that set&rsquo;s row &mdash; {l1ways} ways &mdash; and your
      effective cache is <strong>{l1ways} &times; {l1line} = {l1eff} bytes</strong>. Not 48 KB.
      The other {l1sets_1} rows of the grid are still there, still powered, and completely
      unreachable by this loop, so you are running in <strong>1/{l1sets} of the cache</strong>
      &mdash; {l1frac:.1f}% of it. That is the general shape of the thing: a loop confined to one
      set gets <code>ways &times; line</code>, which is the capacity divided by the number of
      sets, and the more sets a cache has the worse the collapse is when you hit this.
    </p>
    <p>
      Which is why the penalty is as large as it is. A column of a 1024&times;1024 array of
      doubles is 8 KB of data sitting on 1024 separate lines &mdash; it would fit in L1 four times
      over if it were spread across the sets. Confined to one set, {l1ways} of those 1024 lines
      can be resident at a time, so by the time the loop comes back around every one of them has
      been evicted. The array did not get too big for the cache; the loop got too narrow for it.
    </p>
  </section>

  <section>
    <h2><span class="num">08</span>What this means for your loops</h2>
    <div class="tablewrap">
      <table>
        <thead><tr><th scope="col">If your loop&hellip;</th><th scope="col">then&hellip;</th></tr></thead>
        <tbody>
          <tr><td>walks memory with stride 1</td><td>the prefetcher hides most of the latency &mdash; the single biggest factor</td></tr>
          <tr><td>has an inner working set under 48 KB</td><td>it runs at {l1_cyc:.0f} cycles per access; this is the tile to aim for</td></tr>
          <tr class="hi"><td>is already cache-resident</td><td>vectorising is worth up to {ratio16:.1f}&times;; widen the registers</td></tr>
          <tr class="bad"><td>streams from DRAM</td><td>vectorising is worth {ratiodr:.1f}&times;; fix the pattern or the blocking first</td></tr>
          <tr><td>reads one field of a struct per iteration</td><td>it uses 1/8 of every line &mdash; consider structure-of-arrays</td></tr>
          <tr><td>indexes a power-of-two 2-D array by column</td><td>pad the leading dimension before anything else</td></tr>
          <tr><td>uses vector loads</td><td>align the base to 64 bytes, or every load crosses a line</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <footer class="colophon">
    <h2>How this was measured</h2>
    <p>
      Pinned to cpu0 with the core warmed by high-IPC work before every run, because the governor
      raises the clock in response to instructions retiring rather than to the core being busy
      &mdash; an unwarmed core sits near 3.5 GHz and returns a stable, reproducible, wrong number.
      Latency comes from a randomised pointer chase, so nothing can be prefetched; the width and
      alignment figures come from hand-written intrinsics with four accumulators, so the loop
      measures memory rather than the adder&rsquo;s latency.
    </p>
    <p>
      This page is <em>generated</em> from the <code>.csv</code> files by
      <code>make_1core_page.py</code> &mdash; every figure and number above follows the data, so a
      re-measurement does not need the page edited by hand. Sources and raw results are in this
      directory; the notebook version is <code>1_core_cache.ipynb</code>.
    </p>
    <p>
      For the rest of the machine &mdash; coherence, false sharing, what threads cost each other
      &mdash; see <a href="{cmap}">Strix Point Cache Map</a>, and
      <a href="{tlb}">The TLB Cliff</a> for address translation.
    </p>
  </footer>'''

def render(standalone):
    links = (dict(cmap="cache_map.html", why="why_1024_is_slow.html",
                  tlb="tlb_cliff.html", block="block_diagram.html")
             if standalone else
             dict(cmap="https://claude.ai/code/artifact/9bd7bd68-c6d8-4cdb-91df-ad4c61fb9358",
                  why="https://claude.ai/code/artifact/8f277708-7432-4cc5-b15e-7e06c44bc05f",
                  tlb="https://claude.ai/code/artifact/90740681-31c3-4ecd-9420-fb7a5a502069",
                  block="https://claude.ai/code/artifact/bdbbfc9c-1460-4ab0-acf6-395485cc96b1"))
    f = dict(VALS, **FIGS, **links)
    f.update(l1sets=L1_SETS, l1ways=L1_WAYS, l1line=L1_LINE, l1bytes=f'{L1_BYTES:,}',
             l1off=L1_OFF, l1idx=L1_IDX, l1wrap=L1_WRAP,
             l1eff=L1_WAYS*L1_LINE, l1sets_1=L1_SETS-1, l1frac=100/L1_SETS,
             w16_s=VALS["w16"][0], w16_2=VALS["w16"][1], w16_512=VALS["w16"][2],
             w1m_s=VALS["w1m"][0], w1m_2=VALS["w1m"][1], w1m_512=VALS["w1m"][2],
             w8m_s=VALS["w8m"][0], w8m_2=VALS["w8m"][1], w8m_512=VALS["w8m"][2],
             wdr_s=VALS["wdr"][0], wdr_2=VALS["wdr"][1], wdr_512=VALS["wdr"][2],
             ratio1m=VALS["w1m"][2]/VALS["w1m"][0], ratio8m=VALS["w8m"][2]/VALS["w8m"][0])
    body = BODY.format(**f)
    if not standalone:
        return HEAD + "\n\n<div class=\"wrap\">\n\n" + body + "\n\n</div>\n"
    return ("<!doctype html>\n<html lang=\"en\">\n<head>\n"
            '<meta charset="utf-8">\n'
            '<meta name="viewport" content="width=device-width,initial-scale=1">\n'
            "<!-- One Core's View -- GENERATED by make_1core_page.py from the .csv files\n"
            "     in this directory. Do not hand-edit; run `make page` instead. -->\n"
            + HEAD + "\n</head>\n<body>\n<div class=\"wrap\">\n\n" + body
            + "\n\n</div>\n</body>\n</html>\n")

if __name__ == "__main__":
    sys.stdout.write(render(standalone="--standalone" in sys.argv))
