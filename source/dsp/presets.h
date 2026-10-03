// MAZO - factory presets. Plain C++ so the automated checks can play every one of them.
// Names are UTF-8 (Spanish). Each preset starts from the defaults and changes what it needs.
#pragma once
#include "engine.h"

namespace mazo
{
struct FactoryPreset
{
    const char* name;       // UTF-8
    const char* style;      // shown next to the name
    const char* about;
    void (*apply) (Params&);
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list {
        { "Inicio", "Init", "The plain starting kick: clean, Rumble and Distortion off.",
          [] (Params&) {} },

        { "Novecientos", "Clásico", "909-style: pulse click, triangle body, short tail.",
          [] (Params& p) { p.source = pulseSrc; p.clickToneHz = 3000; p.clickLevelDb = -6; p.clickDecayMs = 8;
                           p.wave = triWave; p.pitchAmt = 24; p.sweepMs = 45; p.curve = 0.4f; p.holdMs = 20; p.decayMs = 280;
                           p.subLevelDb = -14; p.subOctave = 0; p.blend = 0.3f; p.warmth = 0.3f; p.compAmount = 0.35f; p.compAttackMs = 15; } },

        { "Golpe", "Clásico", "Tight and punchy: deep fast sweep, slow-attack compression lets the front through.",
          [] (Params& p) { p.pitchAmt = 40; p.sweepMs = 22; p.curve = 0.85f; p.holdMs = 40; p.decayMs = 260; p.clickLevelDb = -5; p.clickToneHz = 4000;
                           p.subLevelDb = -8; p.compAmount = 0.5f; p.compAttackMs = 30; } },

        { "Rodillo Hipnótico", "Rumble", "Hypnotic Roll: rolling in-key rumble that swells between kicks (1/8).",
          [] (Params& p) { p.decayMs = 260; p.holdMs = 40; p.rumbleMix = 0.5f; } },

        { "Túnel", "Rumble", "Long, gritty tunnel rumble that breathes on the quarter note.",
          [] (Params& p) { p.decayMs = 240; p.holdMs = 30; p.pitchAmt = 44; p.sweepMs = 30; p.source = noiseSrc; p.clickLevelDb = -10;
                           p.rumbleMix = 0.65f; p.rumbleDecay = 6.0f; p.rumbleTone = 5; p.rumbleDrive = 0.75f;
                           p.rumbleDuck = 0.9f; p.rumbleDuckTime = 2; p.gainDb = -5; } },

        { "Piso Dub", "Dub", "Dub Floor: soft, dark and long, gently pumping.",
          [] (Params& p) { p.decayMs = 450; p.pitchAmt = 24; p.sweepMs = 60; p.curve = 0.5f; p.rumbleMix = 0.55f; p.rumbleDrive = 0.2f; p.rumbleTone = 2; p.rumbleDecay = 5.0f;
                           p.rumbleDuck = 0.6f; p.rumbleDuckTime = 2; p.clickLevelDb = -14; p.warmthType = 1; p.warmth = 0.4f; p.gainDb = -6; } },

        { "Gruñido Industrial", "Duro", "Industrial Growl: noise transient, fully driven rumble, fast 1/16 ducking.",
          [] (Params& p) { p.source = noiseSrc; p.decayMs = 220; p.holdMs = 30; p.rumbleMix = 0.6f; p.rumbleDrive = 1.0f; p.rumbleTone = 9;
                           p.rumbleDecay = 3.5f; p.rumbleAmount = 0.9f; p.rumbleDuckTime = 0; p.distMix = 0.3f; p.distDrive = 0.7f; p.gainDb = -5; } },

        { "Martillo", "Duro", "Hard techno hammer: long hold, clipped in parallel, Clean Sub underneath.",
          [] (Params& p) { p.wave = softWave; p.holdMs = 80; p.decayMs = 300; p.distMode = 0; p.distDrive = 0.75f; p.distMix = 0.5f; p.distToneHz = 4000;
                           p.compAmount = 0.6f; p.compAttackMs = 20; p.cleanSub = true; p.subLevelDb = -6; p.blend = 0.6f; p.gainDb = -6; } },

        { "Acero", "Duro", "Steel: wave-folded body over a gritty 1/16 rumble, a little brighter.",
          [] (Params& p) { p.distMode = 1; p.distDrive = 0.6f; p.distMix = 0.45f; p.distToneHz = 6000; p.rumbleMix = 0.45f; p.rumbleDrive = 0.9f;
                           p.rumbleDuckTime = 0; p.decayMs = 220; p.compAmount = 0.5f; p.tiltDb = 1.5f; p.gainDb = -6; } },

        { "Terremoto", "Duro", "Earthquake: long soft-clipped boom, heavy sub and rumble.",
          [] (Params& p) { p.wave = softWave; p.holdMs = 120; p.decayMs = 500; p.pitchAmt = 36; p.sweepMs = 50; p.distMix = 0.4f; p.distDrive = 0.6f;
                           p.rumbleMix = 0.6f; p.rumbleDrive = 0.8f; p.compAmount = 0.6f; p.subLevelDb = 0; p.blend = 0.5f; p.subDecayMs = 1400; p.gainDb = -7; } },

        { "Seco", "Mínimo", "Dry minimal: short, airy noise tick, almost no sub.",
          [] (Params& p) { p.decayMs = 180; p.holdMs = 10; p.sweepMs = 18; p.pitchAmt = 30; p.subLevelDb = -20; p.source = noiseSrc; p.clickToneHz = 8000;
                           p.clickLevelDb = -10; p.clickDecayMs = 10; p.compAmount = 0.2f; p.warmth = 0.15f; } },

        { "Chispa", "Mínimo", "Spark: bright click, quick sweep, tilted up.",
          [] (Params& p) { p.clickToneHz = 7000; p.clickLevelDb = -3; p.decayMs = 200; p.holdMs = 15; p.pitchAmt = 30; p.sweepMs = 15;
                           p.subLevelDb = -16; p.tiltDb = 2.0f; } },

        { "Latido", "Mínimo", "Heartbeat: round, soft triangle with a slow gentle sweep.",
          [] (Params& p) { p.wave = triWave; p.pitchAmt = 18; p.sweepMs = 60; p.curve = 0.5f; p.source = noiseSrc; p.clickLevelDb = -18; p.decayMs = 400;
                           p.warmthType = 1; p.warmth = 0.5f; p.compAmount = 0.2f; } },

        { "Pulso Profundo", "Profundo", "Deep pulse: long in-key sub that blooms after the punch.",
          [] (Params& p) { p.subLevelDb = 0; p.subDecayMs = 1800; p.blend = 0.7f; p.holdMs = 60; p.decayMs = 350; p.pitchAmt = 30; p.sweepMs = 35;
                           p.curve = 0.8f; p.warmth = 0.3f; p.gainDb = -6; } },

        { "Sótano", "Profundo", "Warehouse basement: boxy, woody, tube-warm and tilted dark. No rumble.",
          [] (Params& p) { p.decayMs = 240; p.holdMs = 90; p.wave = softWave; p.source = pulseSrc; p.clickToneHz = 1500; p.clickLevelDb = -8;
                           p.warmthType = 1; p.warmth = 0.8f; p.tiltDb = -3.0f; p.compAmount = 0.5f; p.compAttackMs = 25; p.gainDb = -5; } },

        { "Ceniza", "Lo-Fi", "Ash: crushed in parallel, taped, tilted dark.",
          [] (Params& p) { p.distMode = 2; p.distDrive = 0.55f; p.distMix = 0.4f; p.distToneHz = 5000; p.warmth = 0.6f; p.tiltDb = -1.0f; p.gainDb = -5; } },
    };
    return list;
}

// Loads a factory preset. With lockKey on, the Key, Octave and Fine you set for your track stay as they are.
inline void applyFactory (Params& p, int index, bool lockKey)
{
    const auto& list = factoryPresets();
    const int key = p.key, oct = p.octave; const float fine = p.fine;
    p = Params();
    list[(size_t) std::clamp (index, 0, (int) list.size() - 1)].apply (p);
    if (lockKey) { p.key = key; p.octave = oct; p.fine = fine; }
}

} // namespace mazo
