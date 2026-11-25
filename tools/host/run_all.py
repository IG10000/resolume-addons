#!/usr/bin/env python3
"""Render every style of every built plugin with the Linux test host and make one contact sheet per plugin.

  DISPLAY=:99 python3 tools/host/run_all.py build/linux build/linux/shots [plugin-name-filter]
"""
import os, re, subprocess, sys
from PIL import Image, ImageDraw

build, out = sys.argv[1], sys.argv[2]
flt = sys.argv[3] if len(sys.argv) > 3 else ''
os.makedirs(out, exist_ok=True)
host = os.path.join(build, 'imhost')
plugins = sorted(f for f in os.listdir(build) if f.startswith('IMMINENCE_') and f.endswith('.so') and flt in f)
problems = []
for so in plugins:
    path = os.path.join(build, so)
    name = so[len('IMMINENCE_'):-3]
    lst = subprocess.run([host, path, '--list', '--frames', '1', '--out', os.path.join(out, '_tmp')], capture_output=True, text=True)
    m = re.search(r'^\s*\d+ Style .*\((\d+) choices\)', lst.stdout, re.M)
    nstyles = int(m.group(1)) if m else 1
    styles = re.findall(r'value=(.*?) \(\d+ choices\)', lst.stdout.splitlines()[1]) if m else []
    is_mixer = 'type=2' in lst.stdout.splitlines()[0]
    tiles = []
    runs = [(s, None) for s in range(nstyles)] if not is_mixer else [(0, 0.25), (0, 0.5), (0, 0.75)]
    for s, prog in runs:
        tag = '%s_%d' % (name, s) if prog is None else '%s_p%02d' % (name, int(prog * 100))
        args = [host, path, '--frames', '36', '--shots', '35', '--out', os.path.join(out, tag)]
        if nstyles > 1:
            args += ['--style', str(s)]
        if prog is not None:
            args += ['--progress', str(prog)]
        r = subprocess.run(args, capture_output=True, text=True)
        ok = r.returncode == 0 and 'GL state OK' in r.stdout
        if not ok or 'IMMINENCE: ' in r.stderr:
            problems.append((tag, r.returncode, (r.stderr + r.stdout)[-600:]))
        img = os.path.join(out, tag + '_f035.png')
        label = ('style %d' % s) if prog is None else ('progress %.2f' % prog)
        tiles.append((img, label, ok))
    # contact sheet
    cols = 3
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * 480, rows * 270), (20, 20, 20))
    for i, (img, label, ok) in enumerate(tiles):
        if os.path.exists(img):
            sheet.paste(Image.open(img).convert('RGB').resize((480, 270)), ((i % cols) * 480, (i // cols) * 270))
        d = ImageDraw.Draw(sheet)
        d.text(((i % cols) * 480 + 8, (i // cols) * 270 + 250), '%s %s %s' % (name, label, '' if ok else 'FAILED'), fill=(255, 255, 0) if ok else (255, 60, 60))
    sheet.save(os.path.join(out, 'sheet_%s.png' % name))
    print('%-16s %d run(s) %s' % (name, len(tiles), 'ok' if all(t[2] for t in tiles) else 'PROBLEMS'))
for p in problems:
    print('PROBLEM', p[0], p[1], p[2])
sys.exit(1 if problems else 0)
