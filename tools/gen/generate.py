#!/usr/bin/env python3
"""Generate the IMMINENCE plugin sources from the effect definitions exported from the preview.

Input : tools/gen/effects.json  (exported from preview/index.html by tools/gen/export.js)
Output: src/generated/prelude.inc, src/generated/<family>.inc, src/plugins/<family>.cpp

Every control in the preview is a 0..1 float that the shader reads as uP0..uP7. Here we give each one a
plain-language label and, where the value is really a choice, turn it into a dropdown or an on/off toggle
that still feeds the shader the exact float it expects.
"""
import json, os, re, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SRC = json.load(open(os.path.join(ROOT, 'tools', 'gen', 'effects.json')))
EFFECTS = {e['id']: e for e in SRC['EFFECTS']}

# ---------------------------------------------------------------- families
# (plugin file name, FFGL id, Arena name (<=16 chars), kind, description, [(effect id, style label)])
FAMILIES = [
    ('SliceTrace', 'IMSL', 'IM Slice Trace', 'effect',
     'IMMINENCE: liquid light borders for every LED slice.',
     [('slicetrace', 'Liquid'), ('slice-mercury', 'Mercury'), ('slice-neon', 'Neon Liquid'),
      ('slice-surge', 'Surge'), ('slice-braid', 'Braid'), ('slice-spill', 'Spill')]),
    ('Halo', 'IMHA', 'IM Halo', 'effect',
     'IMMINENCE: premium frames for keynotes, award shows and concerts.',
     [('halo', 'Glass Frame'), ('halo-aurora', 'Aurora'), ('halo-corners', 'Corners')]),
    ('Borders', 'IMBO', 'IM Borders', 'effect',
     'IMMINENCE: chrome ribbon and theatre bulb borders.',
     [('chrome', 'Chrome Ribbon'), ('marquee', 'Bulb Marquee')]),
    ('Aura', 'IMAU', 'IM Aura', 'effect',
     'IMMINENCE: energy auras around performers. Works best on a keyed (cut-out) person.',
     [('aura', 'Edge Flow'), ('aura-flame', 'Flame'), ('aura-photo', 'Aura Photo'), ('aura-electric', 'Electric'),
      ('aura-embers', 'Embers'), ('aura-rings', 'Rings'), ('aura-neon', 'Neon Stroke'),
      ('aura-ghost', 'Ghost Trail'), ('aura-radiance', 'Radiance')]),
    ('LiveLooks', 'IMLL', 'IM Live Looks', 'effect',
     'IMMINENCE: camera looks for live feeds.',
     [('cinema', 'Cinema'), ('dream', 'Dream Bloom'), ('vhs', 'VHS Camcorder'), ('popduo', 'Pop Duotone'),
      ('thermal', 'Thermal')]),
    ('LiveFX', 'IMLF', 'IM Live FX', 'effect',
     'IMMINENCE: show-stopping effects for live feeds.',
     [('hologram', 'Hologram'), ('pointcloud', 'Point Cloud'), ('clonewall', 'Clone Wall')]),
    ('Caustics', 'IMCA', 'IM Caustics', 'effect',
     'IMMINENCE: underwater light overlays.',
     [('caustics', 'Classic'), ('caus-rays', 'God Rays'), ('caus-prism', 'Prism'), ('caus-rain', 'Raindrops')]),
    ('Ink', 'IMIN', 'IM Ink', 'effect',
     'IMMINENCE: organic reveal masks that open and close on the beat.',
     [('ink', 'Ink Bloom'), ('ink-smoke', 'Smoke'), ('ink-burn', 'Burn'), ('ink-water', 'Watercolour')]),
    ('Reveal', 'IMRV', 'IM Reveal', 'effect',
     'IMMINENCE: type cut-outs, flipping tiles and particle dissolves.',
     [('typemask', 'Type Cutout'), ('flipgrid', 'Flip Grid'), ('disintegrate', 'Disintegrate')]),
    ('Type', 'IMTY', 'IM Type', 'effect',
     'IMMINENCE: kinetic type over your clip. Type your word in Word.',
     [('type-carousel', 'Carousel'), ('type-wave', 'Wave'), ('type-cylinder', 'Cylinder'),
      ('type-slam', 'Slam'), ('type-echo', 'Echo Stack')]),
    ('TypeSource', 'IMTS', 'IM Type Source', 'source',
     'IMMINENCE: kinetic type generator. Type your word in Word.',
     [('type-carousel', 'Carousel'), ('type-wave', 'Wave'), ('type-cylinder', 'Cylinder'),
      ('type-slam', 'Slam'), ('type-echo', 'Echo Stack')]),
    ('Stylize', 'IMST', 'IM Stylize', 'effect',
     'IMMINENCE: LED wall, contour, edge neon, ASCII, Rutt-Etra and op-art looks.',
     [('led', 'LED Wall'), ('contour', 'Contour'), ('edgeneon', 'Edge Neon'), ('ascii', 'ASCII'),
      ('ruttetra', 'Rutt-Etra'), ('opart', 'Op-Art')]),
    ('Echo', 'IMEC', 'IM Echo', 'effect',
     'IMMINENCE: feedback trails.',
     [('echo', 'Tunnel'), ('echo-kaleido', 'Kaleido'), ('echo-vortex', 'Vortex'), ('echo-stutter', 'Stutter')]),
    ('Melt', 'IMME', 'IM Melt', 'effect',
     'IMMINENCE: liquid feedback that smears, drips, marbles and burns.',
     [('melt', 'Paint'), ('melt-drip', 'Drip'), ('melt-marble', 'Marble'), ('melt-inferno', 'Inferno')]),
    ('Time', 'IMTI', 'IM Time', 'effect',
     'IMMINENCE: time effects built on a frame buffer.',
     [('slitscan', 'Slit Scan'), ('timesplit', 'RGB Time Split'), ('timetiles', 'Time Tiles')]),
    # transitions: one plugin each so they can be picked by name in Arena's transition list
    ('TrLiquidWipe', 'IMT0', 'IM Liquid Wipe', 'mixer', 'IMMINENCE transition: liquid front with a glossy rim.', [('tr-liquid', 'Liquid Wipe')]),
    ('TrFlashBurn', 'IMT1', 'IM Flash Burn', 'mixer', 'IMMINENCE transition: overexposed flash reveal.', [('tr-flash', 'Flash Burn')]),
    ('TrWhipZoom', 'IMT2', 'IM Whip Zoom', 'mixer', 'IMMINENCE transition: radial zoom whip.', [('tr-whip', 'Whip Zoom')]),
    ('TrSliceSlide', 'IMT3', 'IM Slice Slide', 'mixer', 'IMMINENCE transition: sliding strips.', [('tr-slices', 'Slice Slide')]),
    ('TrSmokeReveal', 'IMT4', 'IM Smoke Reveal', 'mixer', 'IMMINENCE transition: smoke dissolve.', [('tr-smoke', 'Smoke Reveal')]),
    ('TrPixelStream', 'IMT5', 'IM Pixel Stream', 'mixer', 'IMMINENCE transition: pixel-sorted streaks.', [('tr-stream', 'Pixel Stream')]),
    ('TrHaloIris', 'IMT6', 'IM Halo Iris', 'mixer', 'IMMINENCE transition: expanding ring of light.', [('tr-iris', 'Halo Iris')]),
    ('TrCubeTurn', 'IMT7', 'IM Cube Turn', 'mixer', 'IMMINENCE transition: 3D cube turn.', [('tr-cube', 'Cube Turn')]),
    ('TrGlowDissolve', 'IMT8', 'IM Glow Dissolve', 'mixer', 'IMMINENCE transition: light dissolve through brightness.', [('tr-luma', 'Glow Dissolve')]),
    ('TrSpectrum', 'IMT9', 'IM Spectrum', 'mixer', 'IMMINENCE transition: RGB channels split and swap.', [('tr-rgb', 'Spectrum Split')]),
]

# ---------------------------------------------------------------- controls
# Choice helpers. Each returns (labels, shader values).
def ints(a, b, fmt='{}'):
    # shader: n = floor(mix(a,b,v)+.5)
    return [fmt.format(n) for n in range(a, b + 1)], [(n - a) / (b - a) for n in range(a, b + 1)]

def ints_floor(a, b, fmt='{}'):
    # shader: n = floor(mix(a,b,v))
    return [fmt.format(n) for n in range(a, b)] , [(n - a + .5) / (b - a) for n in range(a, b)]

def steps(labels, n):
    # shader: k = floor(v*n)
    return labels, [(k + .5) / n for k in range(len(labels))]

BEATS4 = steps(['1 beat', '2 beats', '4 beats', '8 beats'], 3.99)

# (effect id, param index) -> override dict: label / kind ('option'|'toggle'|'slider') / options
OV = {}
def opt(eid, i, label, choice, default_index):
    labels, values = choice
    OV[(eid, i)] = dict(kind='option', label=label, labels=labels, values=values, default=default_index)
def tog(eid, i, label):
    OV[(eid, i)] = dict(kind='toggle', label=label)
def lab(eid, i, label):
    OV[(eid, i)] = dict(kind='slider', label=label)
def hide(eid, i):
    OV[(eid, i)] = dict(kind='hidden')

for sid in ['slicetrace', 'slice-mercury', 'slice-neon', 'slice-surge', 'slice-braid', 'slice-spill']:
    opt(sid, 0, 'Slices', ints(1, 5), 2)
    lab(sid, 1, 'Centre slice width')
    lab(sid, 4, 'Amount of light')
lab('slice-spill', 7, 'Tint (0 = white)')
lab('halo-corners', 5, 'Colour amount')
lab('marquee', 1, 'Bulb count')

for a in ['aura-flame', 'aura-photo', 'aura-electric', 'aura-embers', 'aura-rings', 'aura-neon', 'aura-ghost', 'aura-radiance']:
    lab(a, 7, 'Brightness key')
tog('aura-flame', 6, 'Person in front')
tog('aura-electric', 6, 'Person in front')
opt('aura-neon', 1, 'Strokes', ints(1, 4), 2)
opt('aura-ghost', 0, 'Ghosts', ints(2, 6), 3)
lab('aura', 0, 'Edge sensitivity')

opt('thermal', 1, 'Palette', steps(['Iron', 'Rainbow', 'Arctic'], 2.99), 0)
tog('thermal', 2, 'Black-hot')
lab('thermal', 4, 'Scale and crosshair')
opt('clonewall', 0, 'Grid', ints(2, 5, '{0} x {0}'), 1)
opt('clonewall', 2, 'Colour mode', steps(['Natural', 'Rainbow tiles', 'Pop colours'], 2.99), 2)
tog('clonewall', 5, 'Shuffle on beat')
lab('clonewall', 1, 'Delay per tile')
opt('popduo', 1, 'Colour levels', ints(2, 6), 1)
tog('popduo', 5, 'Swap palette every bar')
tog('vhs', 6, 'REC dot')
lab('hologram', 7, 'Brightness key')
lab('pointcloud', 7, 'Brightness key')
lab('pointcloud', 6, 'Single colour')

opt('caus-rain', 0, 'Drops', steps(['1 per beat', '2 per beat', '4 per beat'], 2.99), 0)
for i in ['ink', 'ink-smoke', 'ink-burn', 'ink-water']:
    opt(i, 0, 'Cycle', BEATS4, 2)
opt('disintegrate', 0, 'Cycle', steps(['2 beats', '4 beats', '8 beats', '16 beats'], 3.99), 2)
opt('typemask', 4, 'Rows', ints(1, 5), 0)
opt('flipgrid', 0, 'Tile rows', ints(2, 10), 2)

for t in ['type-carousel', 'type-wave', 'type-cylinder', 'type-slam', 'type-echo']:
    pass
opt('type-carousel', 0, 'Rows', ints(2, 8), 2)
tog('type-carousel', 3, 'Outline every other row')
opt('type-wave', 0, 'Rows', ints(1, 6), 2)
opt('type-cylinder', 0, 'Drums', ints(1, 4), 1)
opt('type-cylinder', 3, 'Words around drum', ints(1, 4), 3)
opt('type-slam', 0, 'Rows', ints(2, 6), 1)
opt('type-echo', 0, 'Copies', ints(3, 12), 3)
for t, i in [('type-carousel', 5), ('type-wave', 5), ('type-cylinder', 5), ('type-slam', 5), ('type-echo', 6)]:
    lab(t, i, 'Fill letters with clip')

lab('led', 3, 'LED to halftone')
lab('ascii', 1, 'Clip colours to single colour')
lab('ascii', 4, 'Single colour hue')
lab('ruttetra', 4, 'Clip colours to neon')
lab('opart', 4, 'Ink colour (0 = black)')
lab('opart', 5, 'Paper from clip')
lab('contour', 3, 'Black background')

opt('echo-kaleido', 0, 'Segments', ints(3, 12), 2)
opt('echo-stutter', 0, 'Rate', steps(['2 per beat', '4 per beat', '8 per beat'], 2.99), 1)
opt('echo-stutter', 1, 'Copies', ints_floor(2, 6), 3)
for e in ['echo', 'echo-kaleido', 'echo-vortex']:
    idx = [p[0] for p in EFFECTS[e]['params']].index('Key')
    lab(e, idx, 'Cut-through level')
opt('melt-marble', 2, 'Vortices', ints(2, 6), 2)

opt('slitscan', 0, 'Direction', steps(['Left to right', 'Bottom to top', 'From centre', 'Organic'], 3.99), 0)
tog('slitscan', 2, 'Reverse')
opt('timetiles', 3, 'Split depth', steps(['None', 'Once', 'Twice', 'Three times'], 3.99), 2)
opt('timetiles', 5, 'Shuffle', steps(['Every beat', '2 per beat', '4 per beat'], 2.99), 0)

for t in ['tr-liquid', 'tr-flash', 'tr-whip', 'tr-slices', 'tr-smoke', 'tr-stream', 'tr-iris', 'tr-cube', 'tr-luma', 'tr-rgb']:
    hide(t, 0); hide(t, 1)          # Duration / Hold only existed for the preview's auto-play
lab('tr-liquid', 5, 'Rim colour (0 = white)')
lab('tr-iris', 4, 'Ring colour (0 = white)')
lab('tr-luma', 3, 'Edge colour (0 = white)')
opt('tr-slices', 2, 'Slices', ints_floor(3, 12), 2)
tog('tr-slices', 3, 'Horizontal')
tog('tr-stream', 5, 'Right to left')
tog('tr-cube', 3, 'Turn left')
tog('tr-luma', 4, 'Darks first')

# ---------------------------------------------------------------- shaders
PRELUDE = SRC['PRELUDE']
assert PRELUDE.startswith('#version 300 es\nprecision highp float;\nprecision highp sampler2DArray;\n')
PRELUDE = '#version 410 core\n' + PRELUDE[len('#version 300 es\nprecision highp float;\nprecision highp sampler2DArray;\n'):]
PRELUDE = PRELUDE.replace('uniform float uP7;\n', 'uniform float uP7;\nuniform float uBeatAmt;\nuniform float uProgress;\n')
assert 'uBeatAmt' in PRELUDE
PRELUDE = PRELUDE.replace('float kick(){return exp(-uBeat*5.);}', 'float kick(){return exp(-uBeat*5.)*uBeatAmt;}')
assert 'uBeatAmt;}' in PRELUDE
MAIN = SRC['MAIN']
TPROG = re.compile(r'float tprog\(\)\{[^\n]*\}')

def shader_body(eid, kind):
    body = EFFECTS[eid]['glsl']
    if kind == 'mixer':
        assert TPROG.search(body), eid
        body = TPROG.sub('float tprog(){return clamp(uProgress,0.,1.);}', body)
    return body

def flags_for(body):
    f = []
    if 'uPrev' in body or 'prevf(' in body: f.append('IM_FEEDBACK')
    if 'hist(' in body or 'hist4(' in body or 'uHist' in body: f.append('IM_HISTORY')
    if 'uText' in body: f.append('IM_TEXT')
    if 'uGlyph' in body: f.append('IM_GLYPH')
    if 'uSrc2' in body or 'TB(' in body: f.append('IM_SECOND')
    return ' | '.join(f) if f else '0'

def chunks(s, n=6000):
    out = []
    while s:
        cut = s[:n]
        if len(s) > n:
            k = cut.rfind('\n')
            if k > 0: cut = s[:k + 1]
        out.append(cut); s = s[len(cut):]
    return out

def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'

def raw(s):
    assert ')IMSH' not in s
    return 'R"IMSH(' + s + ')IMSH"'

def ident(s):
    return re.sub(r'[^A-Za-z0-9]', '_', s)

GEN = os.path.join(ROOT, 'src', 'generated')
PLUG = os.path.join(ROOT, 'src', 'plugins')
os.makedirs(GEN, exist_ok=True); os.makedirs(PLUG, exist_ok=True)

with open(os.path.join(GEN, 'prelude.inc'), 'w') as f:
    f.write('// Generated by tools/gen/generate.py. Do not edit by hand.\n')
    f.write('static const char* const kImPrelude[] = {\n')
    for c in chunks(PRELUDE): f.write('  ' + raw(c) + ',\n')
    f.write('  nullptr };\n')
    f.write('static const char* const kImMain = ' + raw(MAIN) + ';\n')

catalog = []
for fname, fid, aname, kind, desc, styles in FAMILIES:
    assert len(aname) <= 16, aname
    lines = ['// Generated by tools/gen/generate.py. Do not edit by hand.', '#pragma once', '#include "core/ImDefs.h"', '']
    style_rows = []
    for si, (eid, slabel) in enumerate(styles):
        e = EFFECTS[eid]
        body = shader_body(eid, kind)
        sym = ident(fname + '_' + eid)
        lines.append('static const char* const kSrc_%s[] = {' % sym)
        for c in chunks(body): lines.append('  ' + raw(c) + ',')
        lines.append('  nullptr };')
        prow = []
        for pi, (pname, pdef) in enumerate(e['params']):
            ov = OV.get((eid, pi), dict(kind='slider', label=pname))
            if ov['kind'] == 'hidden':
                continue
            label = ov.get('label', pname)
            if ov['kind'] == 'option':
                osym = 'kOpt_%s_%d' % (sym, pi)
                lines.append('static const char* const %s_l[] = {%s};' % (osym, ', '.join(cstr(x) for x in ov['labels'])))
                lines.append('static const float %s_v[] = {%s};' % (osym, ', '.join('%.6ff' % v for v in ov['values'])))
                dflt = min(range(len(ov['values'])), key=lambda k: abs(ov['values'][k] - pdef))
                prow.append('  { %s, %d, IM_OPTION, %d.0f, %d, %s_l, %s_v }' % (cstr(label), pi, dflt, len(ov['labels']), osym, osym))
            elif ov['kind'] == 'toggle':
                prow.append('  { %s, %d, IM_TOGGLE, %.1ff, 0, nullptr, nullptr }' % (cstr(label), pi, 1.0 if pdef > .5 else 0.0))
            else:
                prow.append('  { %s, %d, IM_SLIDER, %.4ff, 0, nullptr, nullptr }' % (cstr(label), pi, float(pdef)))
        lines.append('static const ImParamDef kPar_%s[] = {' % sym)
        lines.append(',\n'.join(prow))
        lines.append('};')
        style_rows.append('  { %s, %s, kSrc_%s, kPar_%s, %d, %s }' % (cstr(slabel), cstr(e['desc']), sym, sym, len(prow), flags_for(body)))
        catalog.append((aname, slabel, e['desc'], [OV.get((eid, pi), dict(label=p[0])).get('label', p[0]) for pi, p in enumerate(e['params']) if OV.get((eid, pi), {}).get('kind') != 'hidden']))
    lines.append('static const ImStyleDef kStyles_%s[] = {' % fname)
    lines.append(',\n'.join(style_rows))
    lines.append('};')
    kconst = {'effect': 'IM_KIND_EFFECT', 'source': 'IM_KIND_SOURCE', 'mixer': 'IM_KIND_MIXER'}[kind]
    lines.append('static const ImFamilyDef kFamily = { %s, %s, %s, %s, kStyles_%s, %d };' % (cstr(fid), cstr(aname), cstr(desc), kconst, fname, len(styles)))
    open(os.path.join(GEN, fname + '.inc'), 'w').write('\n'.join(lines) + '\n')
    open(os.path.join(PLUG, fname + '.cpp'), 'w').write(
        '// Generated by tools/gen/generate.py. Do not edit by hand.\n'
        '#include "core/ImPlugin.h"\n#include "generated/%s.inc"\nIM_DEFINE_PLUGIN( kFamily )\n' % fname)

# plugin list for CMake
open(os.path.join(GEN, 'plugins.cmake'), 'w').write(
    '# Generated by tools/gen/generate.py\nset(IM_PLUGINS\n' + '\n'.join('  ' + f[0] for f in FAMILIES) + '\n)\n')

# catalog for the README
with open(os.path.join(GEN, 'catalog.json'), 'w') as f:
    json.dump([dict(plugin=a, style=s, desc=d, controls=c) for a, s, d, c in catalog], f, indent=1)

# standalone shaders for validation
VAL = os.path.join(ROOT, 'build', 'shader-check')
os.makedirs(VAL, exist_ok=True)
for fname, fid, aname, kind, desc, styles in FAMILIES:
    for eid, slabel in styles:
        open(os.path.join(VAL, '%s__%s.frag' % (fname, eid)), 'w').write(PRELUDE + shader_body(eid, kind) + MAIN)

# sources used by the Linux test host to feed the plugins (the same Stage cam / Neon loop as the preview)
HOST = os.path.join(ROOT, 'tools', 'host')
os.makedirs(HOST, exist_ok=True)
with open(os.path.join(HOST, 'sources_gen.h'), 'w') as f:
    f.write('// Generated by tools/gen/generate.py\n#pragma once\n')
    f.write('static const char* const kHostPrelude[] = {\n')
    for c in chunks(PRELUDE): f.write('  ' + raw(c) + ',\n')
    f.write('  nullptr };\n')
    f.write('static const char* kHostMain = ' + raw(MAIN) + ';\n')
    for name in ['stage', 'loop']:
        f.write('static const char* const kHostSrc_%s[] = {\n' % name)
        for c in chunks(SRC['SOURCES'][name]): f.write('  ' + raw(c) + ',\n')
        f.write('  nullptr };\n')

# docs/CONTROLS.md: every plugin, style and control, for the user guide
DOCS = os.path.join(ROOT, 'docs'); os.makedirs(DOCS, exist_ok=True)
with open(os.path.join(DOCS, 'CONTROLS.md'), 'w') as f:
    f.write('# Every IMMINENCE plugin, style and control\n\n')
    f.write('Generated from the plugin tables. Every plugin also has **Beat punch** (how hard beat-synced moves hit; not on transitions), '
            '**Mix** (effects only: 0% = untouched clip, 100% = full effect), **Quality** (High / Medium / Low; lower is lighter on the '
            'graphics card) and **Unlock code**. Transitions also have **Opacity**, which Arena moves for you during a transition.\n')
    last = None
    for fname, fid, aname, kind, desc, styles in FAMILIES:
        f.write('\n## %s\n\n%s\n\n' % (aname, desc.replace('IMMINENCE: ', '').replace('IMMINENCE transition: ', '')))
        for eid, slabel in styles:
            e = EFFECTS[eid]
            ctrls = []
            for pi, (pname, pdef) in enumerate(e['params']):
                ov = OV.get((eid, pi), dict(kind='slider', label=pname))
                if ov['kind'] == 'hidden': continue
                lbl = ov.get('label', pname)
                if ov['kind'] == 'option': lbl += ' (' + ' / '.join(ov['labels']) + ')'
                if ov['kind'] == 'toggle': lbl += ' (on/off)'
                ctrls.append(lbl)
            name = slabel if len(styles) > 1 else 'Controls'
            f.write('- **%s**: %s  \n  _Controls:_ %s\n' % (name, e['desc'], ', '.join(ctrls)))
        if any(EFFECTS[eid]['glsl'].find('uText') >= 0 for eid, _ in styles):
            f.write('\n_Text styles also show **Word** (type anything) and **Font**._\n')
print('families', len(FAMILIES), 'styles', sum(len(f[5]) for f in FAMILIES))
