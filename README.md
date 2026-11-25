# IMMINENCE VJ Pack for Resolume Arena

25 plugins for **Resolume Arena 7 on Windows**: borders, auras, live-camera looks, overlays, masks, kinetic type,
feedback, time effects and transitions. Every effect follows Arena's BPM.

- **Download:** go to [Releases](../../releases), open the newest release, download **IMMINENCE-Windows.zip**.
- **Every style and control:** [docs/CONTROLS.md](docs/CONTROLS.md)
- **Try the looks in a browser first:** open [`preview/index.html`](preview/index.html) (download it and double-click).

---

## What's inside

| In Arena | What it is | Styles |
|---|---|---|
| **IM Slice Trace** | Effect: liquid light borders for each LED slice | Liquid, Mercury, Neon Liquid, Surge, Braid, Spill |
| **IM Halo** | Effect: premium frames for keynotes and concerts | Glass Frame, Aurora, Corners |
| **IM Borders** | Effect: classic borders | Chrome Ribbon, Bulb Marquee |
| **IM Aura** | Effect: energy around a performer | Edge Flow, Flame, Aura Photo, Electric, Embers, Rings, Neon Stroke, Ghost Trail, Radiance |
| **IM Live Looks** | Effect: camera looks for live feeds | Cinema, Dream Bloom, VHS Camcorder, Pop Duotone, Thermal |
| **IM Live FX** | Effect: show-stoppers for live feeds | Hologram, Point Cloud, Clone Wall |
| **IM Caustics** | Effect: underwater light | Classic, God Rays, Prism, Raindrops |
| **IM Ink** | Effect: reveals that open and close on the beat | Ink Bloom, Smoke, Burn, Watercolour |
| **IM Reveal** | Effect: cut-outs and dissolves | Type Cutout, Flip Grid, Disintegrate |
| **IM Type** | Effect: kinetic type over your clip | Carousel, Wave, Cylinder, Slam, Echo Stack |
| **IM Type Source** | Source: kinetic type on its own | Carousel, Wave, Cylinder, Slam, Echo Stack |
| **IM Stylize** | Effect: looks | LED Wall, Contour, Edge Neon, ASCII, Rutt-Etra, Op-Art |
| **IM Echo** | Effect: feedback trails | Tunnel, Kaleido, Vortex, Stutter |
| **IM Melt** | Effect: liquid feedback | Paint, Drip, Marble, Inferno |
| **IM Time** | Effect: time tricks | Slit Scan, RGB Time Split, Time Tiles |
| **IM Liquid Wipe**, **IM Flash Burn**, **IM Whip Zoom**, **IM Slice Slide**, **IM Smoke Reveal**, **IM Pixel Stream**, **IM Halo Iris**, **IM Cube Turn**, **IM Glow Dissolve**, **IM Spectrum** | Transitions | one each |

---

## Install (Windows)

1. **Close Arena.**
2. **Download** `IMMINENCE-Windows.zip` from [Releases](../../releases).
3. **Unblock it** (Windows sometimes blocks downloaded plugins): right-click the zip → **Properties** → tick
   **Unblock** → **OK**.
4. **Unzip it.** Inside the `IMMINENCE` folder are 25 files that end in `.dll`.
5. **Copy all the `.dll` files** into Arena's plugin folder: `Documents\Resolume Arena\Extra Effects`.
   If the folder isn't there, make it. Arena's Preferences show the folder it reads extra plugins from, if yours is
   somewhere else.
6. **Open Arena.** Type **IM** in the search box of the Effects, Sources or transition lists to see them all.

**Updating:** download the new zip and copy the new `.dll` files over the old ones (close Arena first).

---

## Unlock

Without the unlock code everything works, but the picture has a faint **IMMINENCE** watermark across it.

1. Add any IMMINENCE plugin (for example **IM Halo**) to a clip.
2. Scroll to the bottom of its controls and find **Unlock code**.
3. Type your code and press **Enter**.

That's it: **every** IMMINENCE plugin on this computer unlocks straight away, and stays unlocked after restarts. The
box then shows **Unlocked**. Do this once on each computer.

The unlock is saved in `%APPDATA%\IMMINENCE\unlock.key`. Delete that file to lock the computer again.

---

## How it works in Arena

Think of your show like a stack of see-through sheets of glass:

- **Clip:** one video in one box of Arena's grid.
- **Layer:** one sheet of glass. It plays one clip at a time, and layers stack on top of each other.
- **Composition:** the whole stack. It's everything you see in the end.
- **Slices:** in **Advanced Output**, slices cut pieces out of the composition and send them to parts of your LED wall.

### Where to put an effect

- **On a clip:** only that clip gets it. For example, Hologram on your camera clip.
- **On a layer:** every clip played in that layer gets it. For example, Cinema Look on your camera layer.
- **On the composition:** everything gets it, on top. This is where borders like **Slice Trace** and **Halo** go.

Effects run top to bottom, like a recipe. Put a key (cut-out) effect **before** an Aura effect so the aura grows around
the cut-out person.

### Slice Trace and your LED slices

Plugins can't see Arena's slices, so you tell Slice Trace your layout:

- **Slices:** how many LED sections, side by side (1 to 5).
- **Centre slice width:** how much wider the middle section is (for left / big centre / right stages).
- **Inset** and **Corner radius:** where the line sits inside each slice, and how round its corners are.

Put it on the composition, then line it up with your slices once.

### Transitions

Each transition is its own entry, so you pick it by name:

- **When you trigger clips:** select a layer, open its **Transition** settings, set the time, and choose a transition
  such as **IM Liquid Wipe**. Every new clip in that layer now comes in with it. Triggering a whole column uses each
  layer's own transition.
- **With the crossfader:** put layers on side A and side B, choose an IMMINENCE transition as the crossfader's blend,
  and move the fader. Slow move = slow transition. You can map it to a MIDI fader.
- **As a blend mode:** Arena may also list them as layer blend modes, where they stay frozen part-way. For example,
  Spectrum at half-way gives a colour-split look.

Arena moves the transition from start to end for you. It always lands exactly on the old clip at the start and the new
clip at the end.

### The controls

Every plugin uses the same layout, so once you know one you know them all:

- **Style:** pick the look. The controls below change to match the style you picked.
- **Look controls:** the style's own sliders, dropdowns and switches.
- **Word / Font:** for text styles. Type anything into **Word**.
- **Beat punch:** how hard the beat hits. 0% = no beat pulses, 100% = full punch. Everything follows Arena's BPM
  and **Tap**.
- **Mix:** 0% = the untouched clip, 100% = the full effect.
- **Quality:** High, Medium or Low. Use Medium or Low on huge LED canvases or older laptops.
- **Unlock code:** see above.

Every slider can be mapped to MIDI, OSC or the keyboard, animated, or put on the Dashboard, like any Arena control.

### Tips

- **Aura** looks best on a person who has been cut out (keyed). On a normal camera feed, raise **Brightness key** until
  only the person is picked up.
- **Feedback** effects (Echo, Melt, Aura Flame and Edge Flow) and **Time** effects remember the last few moments. They
  start fresh when you change style.
- **Text** uses fonts already on your computer: Arial Black, Impact, Segoe UI Black, Bahnschrift or Arial Bold.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Plugins don't show up in Arena | Check the `.dll` files are directly inside the Extra Effects folder (not in a sub-folder), unblock the zip, restart Arena. |
| Watermark still shows | Type the unlock code again in any IMMINENCE plugin and press Enter. Check that `%APPDATA%\IMMINENCE\unlock.key` exists. |
| An effect shows the plain clip | That style couldn't start on this graphics card. Update the graphics driver and tell us which style. |
| Slow on a big LED canvas | Set **Quality** to Medium or Low. |

---

## For developers

The plugins are built from the same shaders as the browser preview.

```
preview/index.html          browser preview (the original design of every effect)
tools/gen/export.js         pulls every effect out of the preview into tools/gen/effects.json
tools/gen/generate.py       turns effects.json into the plugin tables, shaders and docs/CONTROLS.md
src/core/                   the plugin engine (controls, styles, beat, rendering, unlock, watermark, text)
src/plugins/, src/generated/  generated: one small file per plugin
tools/host/                 Linux test host that loads the plugins like Arena does and saves screenshots
external/                   Resolume FFGL SDK, GLEW, stb (with their licences)
```

**Windows build** (what GitHub runs on every push, then publishes to Releases):

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

**Linux test build** (needs GLEW, GLFW and an X display, for example Xvfb):

```
cmake -S . -B build/linux -DIM_BUILD_TEST_HOST=ON && cmake --build build/linux -j
DISPLAY=:99 python3 tools/host/run_all.py build/linux build/linux/shots
```

**Changing an effect:** edit it in `preview/index.html`, then run
`node tools/gen/export.js preview/index.html tools/gen/effects.json` and `python3 tools/gen/generate.py`.

Licences of included code: see [docs/THIRD-PARTY.md](docs/THIRD-PARTY.md).
