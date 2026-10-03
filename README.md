# MAZO

A kick-only synth VST3 for Ableton Live, by ZOONIDO. Inspired by techno kick
machines, with one big difference: the kick is tuned to a Key, with Fine
tune in cents, and the pitch sweep always lands exactly on that note.
GitHub builds it in the cloud every time files are uploaded, the same way as
the other plugins.

**Current stage: 5 - the designed window.**
Working: Tuning (Key, Octave 0-2, Fine +/-50 ct, fixed key: any MIDI note
fires the kick), the Transient (Noise / Click / Pulse with Decay, Tone,
Level), the Body (Sine / Tri / Soft, Pitch Amount, Sweep, Curve, Attack,
Hold, Decay, Level), the Sub/Tail (Unison or -1 Oct, Attack, Decay, Level,
Blend), Velocity on/off, Output Gain and a safety Ceiling. Stage 2 adds
Rumble on the Sub/Tail: a key-tuned reverb, Drive, a Tone low pass that
follows the Key, and Duck synced to the Live tempo. Stage 3 adds the FX
chain: Warmth (Tape / Tube), Distortion (Clip / Fold / Crush, parallel),
Compression, Low Cut, Tilt and a look-ahead limiter, plus Clean Sub. Stage 4
adds 16 factory presets, your own presets and a preset bar. Stage 5 is the
designed window from the approved mockup: live waveform with the pitch curve,
the tuning panel with its note grid, every section with knobs and switches,
and the OUT meter. It opens at 1024 x 674 and scales from 768 to 1600 wide.

## Where things live

- `source/dsp/engine.h` - the sound: tuning, transient, body, sub, envelopes,
  Rumble, FX chain.
- `source/dsp/presets.h` - the 16 factory presets.
- `source/plugin_processor.*` - the plugin shell Ableton talks to:
  parameters, MIDI timing, saving state.
- `source/preset_manager.*` - loading, saving, browsing presets.
- `source/plugin_editor.*` - the window.
- `source/ui_look.h` - colours, fonts and how knobs and buttons are drawn.
- `resources/fonts` - the window's fonts (Archivo, JetBrains Mono; SIL Open
  Font License, the licences are next to them).
- `tests/tests.cpp` - automated sound checks, run on every upload. Also
  renders `mazo_preview.wav` from this exact code.
- `.github/workflows/build.yml` - the cloud build recipe (checks on Linux,
  then the Mac build).

## What each GitHub build gives you

In the **Actions** tab, open the newest run:

- **mazo-vst3-mac** - the plugin (Apple Silicon + Intel).
- **mazo-preview** - the MP3 preview, to hear changes before installing.
- The run's summary page lists every sound check as PASS or FAIL. If a check
  fails, the Mac build is skipped on purpose.

## Installing a new build

1. Quit Ableton.
2. Unzip `mazo-vst3-mac.zip` until `MAZO.vst3` appears.
3. Drag `MAZO.vst3` into `/Library/Audio/Plug-Ins/VST3` (replace the old one).
4. In Terminal:
   ```
   xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/MAZO.vst3
   ```
   No message means it worked. If it says "Permission denied", put `sudo `
   in front and enter your Mac password.
5. Open Live, hold **Option** and click **Rescan** in Preferences > Plug-Ins,
   then load a **fresh** instance of MAZO (under ZOONIDO) on a MIDI track.

## Presets

- **Factory (16):** Inicio, Novecientos, Golpe (Clásico) - Rodillo Hipnótico,
  Túnel (Rumble) - Piso Dub (Dub) - Gruñido Industrial, Martillo, Acero,
  Terremoto (Duro) - Seco, Chispa, Latido (Mínimo) - Pulso Profundo, Sótano
  (Profundo) - Ceniza (Lo-Fi).
- **Lock Key** (on by default): loading a preset keeps the Key, Octave and
  Fine you set for your track. Switch it off to take the preset's tuning.
- **Save** stores the current sound as your own preset in
  `~/Music/MAZO/Presets` (a `.mazopreset` file); it asks before replacing
  one with the same name. **Delete** removes your preset's file (the sound
  stays loaded). Factory presets can't be overwritten.
- **< >** step through the factory presets, then yours.
- The Live Set remembers every knob, the preset name and Lock Key.

## Using the window

- Drag a knob up/down to turn it; double-click it to reset. Its value shows
  underneath.
- Click a note in the tuning grid to set the Key, and 0 / 1 / 2 for the
  Octave. The big readout shows the note, its exact Hz and the Fine offset.
- The waveform redraws as you turn knobs: the orange line is the kick, the
  dashed line is the pitch sweep landing on the Key (the dotted line).
- Drag the corner to resize; the window keeps its shape.

## What to listen for

- **Tuning:** set Key to your track's key. Play a bass or pad note on the
  same key and the kick's tail sits right on it. Fine nudges it in cents.
- **Pitch Amount + Sweep + Curve:** the "boom" shape. More Amount = bigger
  drop, longer Sweep = slower drop, higher Curve = fast drop then a long
  settle. Whatever you do, the end of the sweep is exactly the Key.
- **Hold:** keeps the body at full level before it decays - the flat,
  loud "wall" of hard techno kicks.
- **Blend:** at 0% the sub sits under the whole kick; turn it up and the sub
  gets out of the way of the punch and only blooms as the body fades.
- **Velocity:** off (default) = every hit identical, like a drum machine.
- **Transient:** Click is a short tick (an impulse through a wide band pass),
  Noise is a short burst of air, Pulse is a woody "tok". It sits well under
  the body by default; raise Transient Level for more snap.
- **Rumble:** starts with Mix at 0%, so a fresh MAZO is a clean kick. Turn
  **Rumble Mix** up and the other Rumble knobs are already set for a rolling
  techno rumble. **Amount** feeds more sub in (and pushes the Drive harder),
  **Decay** is the length, **Drive** adds grit, **Tone** is a low pass set as
  a multiple of the Key (Key x4 stays equally bright on any note), **Duck**
  and **Duck Time** make it swell between kicks, locked to the Live tempo.
  The reverb is tuned to the note, so the rumble rings in key. It is fed by
  the Sub and the Body.
- **Signal chain:** Transient + Body + Sub + Rumble -> Warmth -> Distortion ->
  Compression -> Low Cut + Tilt -> Gain -> Limiter. With **Clean Sub** on,
  the sub skips Warmth, Distortion and Compression, so the pure tuned note
  stays under the dirt.
- **Distortion** starts at Mix 0%, like Rumble: turn Mix up to hear it. It's
  parallel, so the clean kick (and its tuning) stays underneath.
- **Compression:** slow Attack lets the front of the kick through (more
  punch); fast Attack squashes it into a sustained wall.
- **Low Cut** starts Off because the -1 Oct sub on low keys sits around
  20-30 Hz. Set it just under your sub note if you want to clean up rumble
  you can't hear.
- **Limiter:** nothing ever goes past the Ceiling. It looks 1.5 ms ahead,
  which Ableton compensates for automatically.
- Rumble lives mostly below 400 Hz: listen on headphones or monitors, not
  phone or laptop speakers.
