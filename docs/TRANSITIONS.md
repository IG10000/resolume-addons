# How to use the IMMINENCE transitions in Arena

This page explains, in simple steps, how to make a transition go **once** from one clip to the next and then stop.

---

## First: what a transition is

A transition is the moment one clip turns into another clip.

- The **old clip** is the one playing now.
- The **new clip** is the one you just clicked.
- The transition plays **one time**, from the old clip to the new clip, and then the new clip just keeps playing.

Our transitions (IM Liquid Wipe, IM Carousel, IM Flash Burn and the rest) **do not loop on their own inside Arena.**
Arena tells them how far along they are, from 0% (all old clip) to 100% (all new clip). When Arena reaches 100%, the
transition is finished and it stops.

> The test page in the browser plays every transition over and over on purpose, so you can look at it. Arena does not.

---

## Way 1: Clicking clips in a layer (the one you'll use most)

This makes a transition happen **every time you click a new clip in that layer**.

1. **Click the name of the layer** (for example **Layer 1**) on the left side of Arena. The layer's settings open.
2. Find the **Transition** part of the layer settings.
3. Set the **time** (Duration), for example **1 second**. This is how long the transition takes.
4. Set the **Blend Mode** of the transition to the IMMINENCE transition you want, for example **IM Liquid Wipe**.
5. Now click a clip in Layer 1. It plays.
6. Click **another clip in Layer 1**. The Liquid Wipe plays **once**, then stops on the new clip. Done.

### Where do I set it? (the important part)

- **You set it on the LAYER, one time.** You do **not** set it on every clip, and you do **not** set it on every column.
- If you set it on **Layer 1**, then **every clip in Layer 1, in every column, uses it, every single time.**
- **Each layer has its own setting.** If you also want transitions on Layer 2, set Layer 2 too. Layers can use
  different transitions. For example, Layer 1 uses IM Carousel and Layer 2 uses IM Flash Burn.
- **Clicking a whole column** (the column button at the top) changes the clip on every layer at the same time. Each
  layer uses **its own** transition setting.
- **Duration 0** means a hard cut (no transition).

### Quick example

You have 5 videos in Layer 1, columns 1 to 5, and you want IM Carousel between all of them:

1. Click **Layer 1**, set Transition time to **1.5 s** and Blend Mode to **IM Carousel**.
2. Click column 1's clip, then column 3's clip, then column 2's clip. Every change slides with the carousel, once each
   time. You never have to touch the setting again.

---

## Way 2: The crossfader (you move it by hand)

Use this when you want to **control the speed with your hand** (mouse, MIDI fader).

1. Put your layers on side **A** or side **B**. Each layer has small **A** and **B** buttons.
2. In the crossfader settings, set the crossfader's blend to an IMMINENCE transition, for example **IM Halo Iris**.
3. Move the crossfader from A to B.
   - **Slow move = slow transition. Fast move = fast transition.**
   - **Stop in the middle = the transition freezes in the middle.**
   - It never moves on its own. It goes exactly where your hand puts it.

---

## Way 3: As a layer Blend Mode (a frozen look, not a transition)

A layer's normal **Blend Mode** (not the Transition one) decides how that layer mixes with the layers under it. You
*can* pick an IMMINENCE transition there, but it doesn't play. It sits frozen at the layer's opacity, like a
picture of the transition stopped part-way. Use it only as a special look.

**If you want a clip-to-clip transition, use Way 1 (the Transition setting), not the Blend Mode.**

---

## "It keeps looping / moving by itself." Check these

| What you see | Why | Fix |
|---|---|---|
| It moves back and forth forever | You're watching the browser test page | That's normal there. In Arena it plays once. |
| It's stuck half-way and never finishes | It's set as the layer's **Blend Mode** (Way 3), or the crossfader is in the middle | Set it as the layer's **Transition** (Way 1), or move the crossfader all the way. |
| It plays again and again without clicking | Something is re-triggering the clip: autopilot, a MIDI/OSC loop, or a timeline | Turn off autopilot or the mapping for that layer. |
| It moves on its own even with nothing clicked | The plugin's **Opacity** control has an animation switched on (BPM sync or timeline) | Open the transition's controls, click **Opacity** and turn its animation off. Opacity is the "how far along" value; Arena moves it for you. |
| No transition, just a hard cut | Transition time is 0, or the clips are on **different layers** | Set a time above 0. Transitions happen between clips **on the same layer**. |

**Still stuck?** Send a screenshot of the layer's Transition settings and of where you put the plugin, and it can be
fixed quickly. Menu names can be worded slightly differently between Arena versions.

---

## All IMMINENCE transitions

| Transition | What happens |
|---|---|
| **IM Carousel** (next update) | The old clip slides out while the new one slides in beside it, like a cinematic carousel with depth, tilt and a floor reflection |
| **IM Liquid Wipe** | A smooth liquid front with a glossy rim washes the new clip in |
| **IM Flash Burn** | The old clip blows out into a bright flash, and the new clip comes out of the light |
| **IM Whip Zoom** | The old clip rockets into the screen, and the new one whips back out |
| **IM Slice Slide** | Strips slide away in turns, carrying the new clip in |
| **IM Smoke Reveal** | The old clip turns to smoke, and the new one forms out of it |
| **IM Pixel Stream** | Rows of pixels streak away and leave the new clip behind |
| **IM Halo Iris** | The new clip opens from the middle inside a ring of light |
| **IM Cube Turn** | Both clips sit on a 3D cube that turns |
| **IM Glow Dissolve** | The new clip burns through the bright parts first, with a glowing edge |
| **IM Spectrum** | Red, green and blue split apart and swap to the new clip |

Every transition starts exactly on the old clip and ends exactly on the new clip, so there's never a jump.
