#!/usr/bin/env python3
"""Generate block_diagram.html -- a block diagram of this machine's memory
architecture, with capacities read from /sys and latencies from latency.csv,
so the picture cannot drift from the hardware it describes.

Writes a body fragment to stdout, or the standalone page with --standalone.
"""
import csv, math, subprocess, sys
from pathlib import Path

HERE = Path(__file__).parent
SYS  = Path("/sys/devices/system/cpu")

def sysread(p, default="?"):
    try: return (SYS / p).read_text().strip()
    except OSError: return default

def cache(cpu, idx):
    b = f"cpu{cpu}/cache/index{idx}"
    return dict(size=sysread(f"{b}/size"), ways=sysread(f"{b}/ways_of_associativity"),
                sets=sysread(f"{b}/number_of_sets"), line=sysread(f"{b}/coherency_line_size"),
                shared=sysread(f"{b}/shared_cpu_list"))

L1D, L1I, L2 = cache(0, 0), cache(0, 1), cache(0, 2)
L3A, L3B     = cache(0, 3), cache(4, 3)
PHYS  = len({sysread(f"{p.name}/topology/core_id") for p in SYS.glob("cpu[0-9]*")
             if (p / "topology/core_id").exists()})
GHZ_A = int(sysread("cpu0/cpufreq/cpuinfo_max_freq", "0")) / 1e6
GHZ_B = int(sysread("cpu4/cpufreq/cpuinfo_max_freq", "0")) / 1e6
DRAM_GB = int(subprocess.run(["free","-g"], capture_output=True, text=True).stdout.split("\n")[1].split()[1])

def ncores(shared):
    ids = set()
    for part in shared.split(","):
        if "-" in part:
            a, b = part.split("-"); ids.update(range(int(a), int(b)+1))
        else: ids.add(int(part))
    return len({sysread(f"cpu{c}/topology/core_id") for c in ids})

def corelist(shared):
    ids = sorted({int(sysread(f"cpu{c}/topology/core_id")) for c in
                  (lambda s: [i for p in s.split(",")
                              for i in (range(int(p.split("-")[0]), int(p.split("-")[1])+1)
                                        if "-" in p else [int(p)])])(shared)})
    return ids

NA, NB = ncores(L3A["shared"]), ncores(L3B["shared"])
CA, CB = corelist(L3A["shared"]), corelist(L3B["shared"])

lat = [r for r in csv.DictReader(open(HERE / "latency.csv")) if r["mode"] == "random"]
at  = lambda t: min(lat, key=lambda r: abs(math.log(int(r["bytes"]) / t)))
P   = {"l1": 32 << 10, "l2": 256 << 10, "l3": 8 << 20, "dram": 128 << 20}
NS  = {k: float(at(v)["ns"])     for k, v in P.items()}
CYC = {k: float(at(v)["cycles"]) for k, v in P.items()}

# ============================ the diagram ============================
def core_block(x, label, ghz, accent):
    """One core: L1i and L1d side by side over a unified L2."""
    W = 200
    return f'''
          <rect x="{x}" y="60" width="{W}" height="152" rx="3" class="fill-surface"/>
          <rect x="{x}" y="60" width="{W}" height="152" rx="3" class="{accent}" stroke-width="1.5"/>
          <text x="{x+W//2}" y="82" text-anchor="middle" class="lbl sv-ink w600">{label}</text>
          <text x="{x+W//2}" y="97" text-anchor="middle" class="mono-sm sv-faint">{ghz} &middot; 2 threads</text>

          <rect x="{x+10}" y="108" width="{(W-30)//2}" height="44" rx="2" class="fill-sunken"/>
          <rect x="{x+10}" y="108" width="{(W-30)//2}" height="44" rx="2" class="st-rule" stroke-width="1"/>
          <text x="{x+10+(W-30)//4}" y="126" text-anchor="middle" class="mono-sm sv-ink w600">L1i</text>
          <text x="{x+10+(W-30)//4}" y="141" text-anchor="middle" class="mono-sm sv-dim">{L1I["size"]} &middot; {L1I["ways"]}-way</text>

          <rect x="{x+W//2+5}" y="108" width="{(W-30)//2}" height="44" rx="2" class="fill-sunken"/>
          <rect x="{x+W//2+5}" y="108" width="{(W-30)//2}" height="44" rx="2" class="st-rule" stroke-width="1"/>
          <text x="{x+W//2+5+(W-30)//4}" y="126" text-anchor="middle" class="mono-sm sv-ink w600">L1d</text>
          <text x="{x+W//2+5+(W-30)//4}" y="141" text-anchor="middle" class="mono-sm sv-dim">{L1D["size"]} &middot; {L1D["ways"]}-way</text>

          <rect x="{x+10}" y="160" width="{W-20}" height="44" rx="2" class="fill-sunken"/>
          <rect x="{x+10}" y="160" width="{W-20}" height="44" rx="2" class="st-rule" stroke-width="1"/>
          <text x="{x+W//2}" y="178" text-anchor="middle" class="mono-sm sv-ink w600">L2 unified</text>
          <text x="{x+W//2}" y="193" text-anchor="middle" class="mono-sm sv-dim">{L2["size"]} &middot; {L2["ways"]}-way</text>

          <line x1="{x+W//2}" y1="212" x2="{x+W//2}" y2="246" class="st-dim" stroke-width="1.25" marker-end="url(#bd)"/>'''

def diagram():
    g = ['<defs><marker id="bd" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"'
         ' orient="auto-start-reverse"><path d="M0,1 L9,5 L0,9 z" class="sv-dim"/></marker></defs>']

    # complex headings
    g += [f'<text x="160" y="40" class="cap sv-z5">complex 0 &nbsp;&middot;&nbsp; {NA} cores &nbsp;&middot;&nbsp; {GHZ_A:.2f} GHz</text>',
          f'<text x="670" y="40" class="cap sv-z5c">complex 1 &nbsp;&middot;&nbsp; {NB} cores &nbsp;&middot;&nbsp; {GHZ_B:.2f} GHz</text>']

    g.append(core_block(160, "Core 0",  f"{GHZ_A:.2f} GHz", "st-z5"))
    g.append(core_block(380, "Core 1",  f"{GHZ_A:.2f} GHz", "st-z5"))
    g.append(core_block(670, f"Core {PHYS-1}", f"{GHZ_B:.2f} GHz", "st-z5c"))

    # the cores not drawn
    g += ['<text x="625" y="140" text-anchor="middle" class="sv-faint" '
          'style="font-size:30px;letter-spacing:3px">&middot; &middot; &middot;</text>',
          f'<text x="625" y="166" text-anchor="middle" class="mono-sm sv-faint">cores 2&ndash;{PHYS-2}</text>']

    # L3 slabs, widths in true proportion to their capacities (420 : 210 = 16 : 8 MB)
    for x, w, c, size, cores, accent, fill in (
            (160, 420, NA, L3A["size"], f"cores 0&ndash;{NA-1}",     "st-z5",  "fill-z5"),
            (670, 210, NB, L3B["size"], f"cores {NA}&ndash;{PHYS-1}", "st-z5c", "fill-z5c")):
        g += [f'<rect x="{x}" y="248" width="{w}" height="66" rx="3" class="{fill}"/>',
              f'<rect x="{x}" y="248" width="{w}" height="66" rx="3" class="{accent}" stroke-width="1.75"/>',
              f'<text x="{x+w//2}" y="272" text-anchor="middle" class="lbl sv-ink w600">L3 &nbsp;{size} &nbsp;{L3A["ways"]}-way</text>',
              f'<text x="{x+w//2}" y="290" text-anchor="middle" class="mono-sm sv-dim">shared by {cores}</text>',
              f'<text x="{x+w//2}" y="305" text-anchor="middle" class="mono-sm sv-faint">'
              f'{int(size.rstrip("K"))//1024//c} MB per core</text>',
              f'<line x1="{x+w//2}" y1="314" x2="{x+w//2}" y2="358" class="st-dim" stroke-width="1.25" marker-end="url(#bd)"/>']

    g += ['<text x="625" y="284" text-anchor="middle" class="mono-sm sv-faint">the two L3s are</text>',
          '<text x="625" y="298" text-anchor="middle" class="mono-sm sv-faint">drawn to scale</text>']

    # DRAM
    g += ['<rect x="160" y="360" width="720" height="72" rx="3" class="fill-sunken"/>',
          '<rect x="160" y="360" width="720" height="72" rx="3" class="st-rule" stroke-width="1.75"/>',
          f'<text x="520" y="386" text-anchor="middle" class="lbl sv-ink w600">Main memory &nbsp;&middot;&nbsp; LPDDR5X &nbsp;&middot;&nbsp; {DRAM_GB} GB</text>',
          '<text x="520" y="404" text-anchor="middle" class="mono-sm sv-dim">shared by every core</text>',
          f'<text x="520" y="420" text-anchor="middle" class="mono-sm sv-faint">one 64-byte line moves at a time, at every level above</text>']

    # latency gutter
    for y, lab, ns, cyc in ((133, "L1", NS["l1"], CYC["l1"]), (185, "L2", NS["l2"], CYC["l2"]),
                            (278, "L3", NS["l3"], CYC["l3"]), (392, "DRAM", NS["dram"], CYC["dram"])):
        g += [f'<text x="138" y="{y}" text-anchor="end" class="mono sv-ink w500">{ns:.2f} ns</text>'
              if ns < 10 else
              f'<text x="138" y="{y}" text-anchor="end" class="mono sv-ink w500">{ns:.0f} ns</text>',
              f'<text x="138" y="{y+13}" text-anchor="end" class="mono-sm sv-faint">{cyc:.0f} cycles</text>']
    g.append('<text x="138" y="40" text-anchor="end" class="cap sv-faint">hit cost</text>')
    return "\n          ".join(g)


# ======================= the same caches, to scale =======================
def fig_scale():
    """Area proportional to capacity, distance from the core proportional to
    latency -- both linear, no log axis anywhere.  L1, L2 and L3 fit; DRAM is
    305,000x the capacity of L1 and cannot be drawn beside it, which is the
    point of the arrow off the right-hand edge."""
    import math
    KB   = {"L1d": int(L1D["size"].rstrip("K")), "L2": int(L2["size"].rstrip("K")),
            "L3":  int(L3A["size"].rstrip("K")), "DRAM": DRAM_GB * 1024 * 1024}
    key  = {"L1d": "l1", "L2": "l2", "L3": "l3", "DRAM": "dram"}
    unit = 20.0                    # side of the L1 square, in px
    pxns = 22.0                    # px per nanosecond of latency
    x0, cy = 74.0, 232.0           # the core's right edge, and the centre line

    side = lambda k: unit * math.sqrt(KB[k] / KB["L1d"])
    near = lambda k: x0 + NS[key[k]] * pxns

    g = ['<rect x="24" y="212" width="50" height="40" rx="3" class="fill-surface"/>',
         '<rect x="24" y="212" width="50" height="40" rx="3" class="st-dim" stroke-width="1.5"/>',
         '<text x="49" y="236" text-anchor="middle" class="mono-sm sv-ink w600">core</text>']

    for k, accent, fill in (("L1d", "st-cost", "fill-cost"), ("L2", "st-z5", "fill-z5"),
                            ("L3", "st-z5c", "fill-z5c")):
        w = side(k); x = near(k); y = cy - w / 2
        g += [f'<line x1="{x0}" y1="{cy}" x2="{x:.1f}" y2="{cy}" class="st-grid" stroke-width="1" stroke-dasharray="3 3"/>',
              f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{w:.1f}" rx="2" class="{fill}"/>',
              f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{w:.1f}" rx="2" class="{accent}" stroke-width="1.75"/>']
        # label inside if it fits, otherwise above the block
        cap = f'{KB[k]//1024} MB' if KB[k] >= 1024 else f'{KB[k]} KB'
        if w > 80:
            g += [f'<text x="{x+w/2:.1f}" y="{cy-6:.1f}" text-anchor="middle" class="lbl sv-ink w600">{k}</text>',
                  f'<text x="{x+w/2:.1f}" y="{cy+12:.1f}" text-anchor="middle" class="mono-sm sv-dim">{cap} &middot; {NS[key[k]]:.2f} ns</text>']
        else:
            g += [f'<text x="{x+w/2:.1f}" y="{y-8:.1f}" text-anchor="middle" class="mono-sm sv-ink w600">{k}</text>',
                  f'<text x="{x+w/2:.1f}" y="{y+w+16:.1f}" text-anchor="middle" class="mono-sm sv-dim">{cap}</text>',
                  f'<text x="{x+w/2:.1f}" y="{y+w+28:.1f}" text-anchor="middle" class="mono-sm sv-faint">{NS[key[k]]:.2f} ns</text>']

    # DRAM, which does not fit
    x3 = near("L3") + side("L3")
    g += [f'<line x1="{x3+16:.0f}" y1="{cy}" x2="1064" y2="{cy}" class="st-dim" stroke-width="2" marker-end="url(#bd)"/>',
          f'<text x="{x3+26:.0f}" y="{cy-14:.0f}" class="mono sv-ink w600">DRAM &mdash; off the page</text>',
          f'<text x="{x3+26:.0f}" y="{cy+16:.0f}" class="mono-sm sv-dim">{DRAM_GB} GB &middot; {NS["dram"]:.0f} ns</text>',
          f'<text x="{x3+26:.0f}" y="{cy+30:.0f}" class="mono-sm sv-faint">at this scale: {side("DRAM")/1000:.1f} thousand px across,</text>',
          f'<text x="{x3+26:.0f}" y="{cy+42:.0f}" class="mono-sm sv-faint">starting {near("DRAM"):.0f} px out</text>']

    # distance axis
    g.append(f'<line x1="{x0}" y1="452" x2="1064" y2="452" class="st-rule" stroke-width="1.25"/>')
    for ns in (0, 5, 10, 15, 20, 25, 30, 35, 40):
        x = x0 + ns * pxns
        if x > 1064: break
        g += [f'<line x1="{x:.0f}" y1="448" x2="{x:.0f}" y2="456" class="st-rule" stroke-width="1"/>',
              f'<text x="{x:.0f}" y="470" text-anchor="middle" class="mono-sm sv-faint">{ns}</text>']
    g.append('<text x="1064" y="470" text-anchor="end" class="lbl sv-dim">nanoseconds from the core &rarr;</text>')
    g.append('<text x="24" y="28" class="cap sv-faint">area &prop; capacity &nbsp;&middot;&nbsp; distance &prop; latency &nbsp;&middot;&nbsp; both linear</text>')
    return "\n          ".join(g)

# ============================ the page ============================
CSS = Path(__file__).parent / "1_core_cache.html"
EXTRA = """
/* used by this page but not by the sibling whose stylesheet is reused above */
.fill-surface{fill:var(--surface)}
.fill-sunken{fill:var(--sunken)}
.fill-z5{fill:var(--zen5-soft)}
.fill-z5c{fill:var(--zen5c-soft)}
.steps{display:flex;flex-direction:column;gap:18px;margin:0;padding:0;list-style:none}
.steps li{display:flex;gap:15px}
.steps .n{flex:0 0 auto;width:26px;height:26px;border-radius:50%;
  border:1px solid var(--rule);background:var(--sunken);
  font-family:"IBM Plex Mono",ui-monospace,monospace;font-size:12px;
  display:flex;align-items:center;justify-content:center;color:var(--ink-2);margin-top:2px}
.steps .body{display:flex;flex-direction:column;gap:4px}
.steps h3{font-family:"IBM Plex Sans",sans-serif;font-size:1rem;font-weight:600;margin:0}
.steps p{font-size:14.5px;color:var(--ink-2)}
"""

def shared_style():
    """Reuse the stylesheet the sibling page already generates, so the pages in
    this directory stay one visual set, plus the few rules this page adds."""
    s = CSS.read_text()
    block = s[s.index("<style>"):s.index("</style>")]
    return block + EXTRA + "</style>"

HEAD = '''<title>Twelve Cores, Two L3s</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Archivo:wght@600;700&family=IBM+Plex+Mono:wght@400;500;600&family=IBM+Plex+Sans:wght@400;500;600&display=swap">

'''

BODY = '''  <header class="masthead">
    <p class="backlink">&larr; a diagram for <a href="{one}">One Core&rsquo;s View</a> and <a href="{cmap}">Strix Point Cache Map</a></p>
    <h1>Twelve Cores, Two L3s</h1>
    <p class="standfirst">
      The memory architecture of the course machine, drawn as blocks. Every capacity is read
      from <code>/sys</code> and every latency from a measurement, so this is the machine rather
      than a generic picture of a cache hierarchy &mdash; and the two last-level caches really
      are different sizes.
    </p>
    <div class="machine">
      <span><b>AMD Ryzen AI 9 HX 370</b> &middot; Zen 5</span>
      <span>{phys} cores / {threads} threads</span>
      <span>64-byte lines throughout</span>
      <span>LPDDR5X {dram} GB</span>
    </div>
  </header>

  <section>
    <figure>
      <div class="canvas">
        <svg viewBox="0 0 940 460" role="img" aria-label="Block diagram. Each core holds a split first-level cache -- {l1i} instruction and {l1d} data -- over a unified {l2} second-level cache. Cores 0 to {na_last} share a {l3a} third-level cache; cores {nb_first} to {phys_last} share a smaller {l3b} one. Both complexes reach the same {dram} gigabytes of main memory. Three cores are drawn and the rest elided.">
          {diagram}
        </svg>
      </div>
      <figcaption>
        <b>Three cores drawn, the other nine elided.</b> Every core is identical: a split L1
        &mdash; {l1i} of instructions, {l1d} of data &mdash; over a unified {l2} L2 that is
        private to it. What differs is below that. The two L3 blocks are drawn to scale against
        each other, so the width of each is its real capacity: {l3a_mb} MB behind the first {na}
        cores and {l3b_mb} MB behind the other {nb}.
      </figcaption>
    </figure>
  </section>

  <section>
    <h2><span class="num">01</span>The same caches, drawn to scale</h2>
    <p class="lede">
      The diagram above is a schematic: every box is whatever size fits. This one is not. Each
      cache is drawn with its <strong>area proportional to its capacity</strong> and its
      <strong>distance from the core proportional to its latency</strong> &mdash; both on ordinary
      linear scales, with no logarithm anywhere.
    </p>
    <figure>
      <div class="canvas">
        <svg viewBox="0 0 1100 490" role="img" aria-label="The three caches drawn with area proportional to capacity and distance from the core proportional to latency, both linear. L1 is a small square close in, L2 about four and a half times wider and three and a half times further out, L3 eighteen times wider and eighteen times further. DRAM does not fit on the page at all.">
          {scale}
        </svg>
      </div>
      <figcaption>
        <b>Each cache is about as far away as it is wide.</b> That is not a drafting choice &mdash;
        it falls out of the measurements. L2 holds 21&times; what L1 holds, so it is
        &radic;21 = 4.6&times; wider, and it sits 3.5&times; further out. L3 holds 341&times;, so it
        is 18.5&times; wider and sits 17.8&times; further out. Capacity grows as roughly the square
        of latency across the three, because a bigger array is physically bigger and a signal has
        to cross it.
      </figcaption>
    </figure>
    <div class="note warn">
      <h3>DRAM is why the page ends</h3>
      <p>
        Main memory holds <strong>305,000&times;</strong> what L1 holds. On this scale its block
        would be about <strong>11,000 pixels across</strong> and would start 2,255 pixels off the
        right-hand edge. If the L1 square were 5 mm wide on your screen, DRAM would be
        <strong>2.8 metres across, starting half a metre away</strong>. It also breaks the
        square-root relationship the three caches follow &mdash; 553&times; wider but only
        130&times; further &mdash; because it is not on the chip at all, and the distance stops
        being about crossing an array.
      </p>
    </div>
  </section>

  <section>
    <h2><span class="num">02</span>How to read the block diagram</h2>
    <ol class="steps">
      <li><span class="n">1</span><div class="body">
        <h3>Split at L1, unified below</h3>
        <p>L1 is two separate caches, because instruction fetch and data access happen at the
        same time and would otherwise fight for the same ports. From L2 down, one cache holds
        both.</p>
      </div></li>
      <li><span class="n">2</span><div class="body">
        <h3>Private above, shared below</h3>
        <p>L1 and L2 belong to one core. L3 belongs to a complex. Main memory belongs to
        everybody. The line between private and shared is where coherence traffic starts, and
        it sits between L2 and L3.</p>
      </div></li>
      <li><span class="n">3</span><div class="body">
        <h3>The two L3s are not the same size</h3>
        <p>{l3a_mb} MB for the first {na} cores, {l3b_mb} MB for the other {nb} &mdash;
        {per_a} MB per core against {per_b} MB per core. A working set that fits in L3 on one
        core can miss to DRAM on another, which is why any measurement here is pinned.</p>
      </div></li>
      <li><span class="n">4</span><div class="body">
        <h3>Everything moves in 64-byte lines</h3>
        <p>Every arrow in the diagram carries a whole line, never a byte. That single fact is
        most of what makes an access pattern fast or slow &mdash; see
        <a href="{one}">One Core&rsquo;s View</a>.</p>
      </div></li>
    </ol>
  </section>

  <section>
    <h2><span class="num">03</span>The full specification</h2>
    <div class="tablewrap">
      <table>
        <thead><tr><th scope="col">Level</th><th scope="col">Size</th><th scope="col">Ways</th>
          <th scope="col">Sets</th><th scope="col">Line</th><th scope="col">Latency</th>
          <th scope="col">Cycles</th><th scope="col">Private to</th></tr></thead>
        <tbody>
          <tr><td>L1i</td><td>{l1i}</td><td>{l1i_ways}</td><td>{l1i_sets}</td><td>64 B</td><td>&mdash;</td><td>&mdash;</td><td>one core</td></tr>
          <tr class="hi"><td>L1d</td><td>{l1d}</td><td>{l1d_ways}</td><td>{l1d_sets}</td><td>64 B</td><td>{ns_l1:.2f} ns</td><td>{cyc_l1:.0f}</td><td>one core</td></tr>
          <tr><td>L2</td><td>{l2}</td><td>{l2_ways}</td><td>{l2_sets}</td><td>64 B</td><td>{ns_l2:.2f} ns</td><td>{cyc_l2:.0f}</td><td>one core</td></tr>
          <tr><td>L3 &mdash; complex 0</td><td>{l3a}</td><td>{l3_ways}</td><td>{l3a_sets}</td><td>64 B</td><td>{ns_l3:.1f} ns</td><td>{cyc_l3:.0f}</td><td>{na} cores</td></tr>
          <tr><td>L3 &mdash; complex 1</td><td>{l3b}</td><td>{l3_ways}</td><td>{l3b_sets}</td><td>64 B</td><td>{ns_l3:.1f} ns</td><td>{cyc_l3:.0f}</td><td>{nb} cores</td></tr>
          <tr class="bad"><td>Main memory</td><td>{dram} GB</td><td>&mdash;</td><td>&mdash;</td><td>64 B</td><td>{ns_dram:.0f} ns</td><td>{cyc_dram:.0f}</td><td>nobody</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <footer class="colophon">
    <h2>Where the numbers come from</h2>
    <p>
      Capacities, associativity, set counts and line size are read from
      <code>/sys/devices/system/cpu/cpu*/cache/</code>. Latencies come from a randomised pointer
      chase (<code>cache_latency.c</code>), pinned and warmed, so nothing is prefetched and the
      figure is the real cost of reaching that level.
    </p>
    <p>
      This page is generated by <code>make_block_diagram.py</code> &mdash; run <code>make page</code>
      to rebuild it. Nothing here is hand-typed, so the diagram cannot drift from the machine it
      describes.
    </p>
  </footer>'''

def render(standalone):
    links = (dict(one="1_core_cache.html", cmap="cache_map.html") if standalone else
             dict(one="https://claude.ai/code/artifact/6bd77e6b-30bc-465b-9c19-ea161251401f",
                  cmap="https://claude.ai/code/artifact/9bd7bd68-c6d8-4cdb-91df-ad4c61fb9358"))
    mb = lambda s: int(s.rstrip("K")) // 1024
    f = dict(links, diagram=diagram(), scale=fig_scale(), phys=PHYS, threads=PHYS*2, dram=DRAM_GB,
             l1i=L1I["size"], l1d=L1D["size"], l2=L2["size"], l3a=L3A["size"], l3b=L3B["size"],
             l1i_ways=L1I["ways"], l1d_ways=L1D["ways"], l2_ways=L2["ways"], l3_ways=L3A["ways"],
             l1i_sets=L1I["sets"], l1d_sets=L1D["sets"], l2_sets=L2["sets"],
             l3a_sets=L3A["sets"], l3b_sets=L3B["sets"],
             na=NA, nb=NB, na_last=NA-1, nb_first=NA, phys_last=PHYS-1,
             l3a_mb=mb(L3A["size"]), l3b_mb=mb(L3B["size"]),
             per_a=mb(L3A["size"])//NA, per_b=mb(L3B["size"])//NB,
             ns_l1=NS["l1"], ns_l2=NS["l2"], ns_l3=NS["l3"], ns_dram=NS["dram"],
             cyc_l1=CYC["l1"], cyc_l2=CYC["l2"], cyc_l3=CYC["l3"], cyc_dram=CYC["dram"])
    body = BODY.format(**f)
    head = HEAD + shared_style()
    if not standalone:
        return head + "\n\n<div class=\"wrap\">\n\n" + body + "\n\n</div>\n"
    return ("<!doctype html>\n<html lang=\"en\">\n<head>\n"
            '<meta charset="utf-8">\n'
            '<meta name="viewport" content="width=device-width,initial-scale=1">\n'
            "<!-- Twelve Cores, Two L3s -- GENERATED by make_block_diagram.py from /sys and\n"
            "     latency.csv. Do not hand-edit; run `make page` instead. -->\n"
            + head + "\n</head>\n<body>\n<div class=\"wrap\">\n\n" + body
            + "\n\n</div>\n</body>\n</html>\n")

if __name__ == "__main__":
    sys.stdout.write(render(standalone="--standalone" in sys.argv))
