# DaliGTR — Architecture v2.2

**Dali Audio · VST3 · Windows x64 + macOS Universal (Intel + Apple Silicon)**

> **v2.2:** כלי הפקה אישי. המשתמש לא מוריד ספריות: DaliGTR מגיע עם **Sound Set** מובנה שנבנה אוטומטית ממקורות פתוחים, ומנוע **Hybrid** שממלא את מה שחסר בהקלטות. עוזר הריפים חי בתוך הפלאגין. DAW יעד: Ableton Live 11/12.

מסמך זה הוא הבסיס לכל המימוש. שום קוד לא נכתב לפני שהמסמך מאושר, וכל שינוי ארכיטקטוני מתעדכן כאן קודם.

---

## 0. עקרונות מנחים

1. **תקרת הריאליזם נקבעת על ידי המקור.** הליבה היא Sample Engine שמנגן **הקלטות אמיתיות של גיטרות חשמליות**, מכמה ספריות שונות, כך שכל ספרייה היא "גיטרה" עם גוון משלה. המודל הפיזיקלי יורד לתפקיד משני: Resonance ומילוי חורים.
2. **החלטות נגינה לפני סאונד.** כל MIDI note עובר דרך Guitar Brain שמחליט *איך* גיטריסט היה מנגן אותו. ה-Sampler מקבל הוראת ביצוע מלאה (מיתר, fret, טכניקה, כיוון פריטה, עוצמה) ולא pitch.
3. **ריאליזם ממבנה, לא מרנדום.** וריאציה מגיעה מ-Round Robin אמיתי, מבחירת fingering, ומ-humanization מתואם וקטן. אין jitter עצמאי על פרמטרים.
4. **חמישה דברים טובים עדיפים על עשרים בינוניים.** כל Phase נסגר רק אחרי שעבר בדיקות אודיו (סעיף 12).
5. **Realtime-safe.** אין הקצאות זיכרון, נעילות או I/O ב-audio thread. טעינת samples ברקע, החלפת ספרייה דרך atomic pointer.

---

## 0.1 Sound Set — הגיטרות של DaliGTR

המקורות נבחרו לפי שלושה קריטריונים: הקלטות אמיתיות של גיטרה חשמלית, הורדה אוטומטית מ-GitHub בלי פעולה של המשתמש, ורישיון פתוח (CC0).

| גיטרה ב-DaliGTR | מקור | מה יש בהקלטות |
|---|---|---|
| **SG** | Karoryfer — Emilyguitar (CC0) | Epiphone SG, DI משני ה-pickups, flatwounds 11. 4 velocity × 3 RR, release noises × 4 RR, fingering / percussive noises × 5 RR |
| **Gretsch** | Karoryfer — Black And Green Guitars (CC0) | Gretsch Anniversary hollowbody |
| **Hofner** | Karoryfer — Black And Green Guitars (CC0) | Hofner Club hollowbody |
| **Archtop** | Karoryfer — Shinyguitar (CC0) | שני ערוצים אמיתיים: magnetic pickup + מיקרופון. 4 velocity × 4 RR |
| **Standard** (אופציונלי) | Unreal Instruments — Standard Guitar | Humbucker DI עם legato, slides, harmonics ו-mutes מוקלטים. לשימוש אישי בלבד |

**מה חסר בהקלטות, ולכן בא מה-Hybrid:** הדגימה היא כל טרצה קטנה (כל 3 fret), בלי מידע על מיתר, ובלי מעברי legato מוקלטים (חוץ מ-Standard).

## 0.2 Hybrid Engine — מה מגיע מאיפה

| שכבה | מקור | מה היא נותנת |
|---|---|---|
| **Tone** | הקלטות אמיתיות | ה-attack של הפריטה, גוף הצליל, הספקטרום לכל עוצמה, הדעיכה, רעשי release ואצבעות אמיתיים |
| **Coverage** | Resampling איכותי | כל fret: נבחרת ההקלטה הקרובה, מוזזת עד ±1.5 semitones, עם **Fret Color**, תיקון ספקטרלי עדין לפי מיקום על הצוואר |
| **Behavior** | מודלים | bends, vibrato ו-slides כמסלולי pitch על ההקלטה עצמה. Legato: כניסה ל-sustain של תו היעד בלי attack, עם crossfade מותאם phase ורעש hammer/pull |
| **Continuation** | Waveguide מוזן מההקלטה | sustain ארוך ו-**Feedback** (צליל שנמשך ומתנפח, קלאסי לגואה). ה-waveguide מקבל את המחזורים האחרונים של ההקלטה וממשיך אותם עם אותו גוון |
| **Space** | מודלים | Sympathetic resonance, pickup transform, amp, cab |

העיקרון: **כל מה שנשמע** (גוון, attack, רעש) מגיע מהקלטה אמיתית. **כל מה שזז** (pitch, מעברים, תהודה) מגיע ממודל. זה ההפך מהגרסה הקודמת, שבה גם הגוון היה סינתטי.

## 0.3 איך ה-Sound Set מגיע לפלאגין (בלי הורדה ידנית)
1. **GitHub Actions** (job `soundset`): `git clone --depth 1` של המקורות, המרה ל-FLAC 48 kHz, הרצת `library_builder` (ניתוח onset, pitch ו-loudness, מיפוי ל-DGL לפי Profile), ואריזה כ-`DaliGTR-SoundSet.zip` ב-Release של ה-repo שלך. נבנה פעם אחת ומתעדכן רק כשמשנים מקורות.
2. **הפלאגין בהפעלה הראשונה** מוריד את ה-Sound Set מה-Release לתיקייה `Documents/Dali Audio/DaliGTR/Sounds`, עם פס התקדמות ובדיקת תקינות. מכאן והלאה הוא עובד offline.
3. **Fallback:** אם אין אינטרנט, כפתור "Locate Sound Set" לקובץ zip מקומי.

גודל משוער: כ-1 GB ב-WAV, כ-500–600 MB ב-FLAC.

---

## 1. Signal Flow

```
 MIDI In ─┐
 Riff Engine ─┼─► INPUT STAGE ──► LOOKAHEAD BUFFER ──► GUITAR BRAIN ──► PerformanceEvents
 Solo Engine ─┤   (keyswitch,       (0–120 ms,          ├ FingeringSolver
 Picking Seq ─┘    CC, channel=      PDC-reported)       ├ HandModel
                   string override)                     ├ PickingEngine
                                                        ├ LegatoEngine
                                                        ├ ArticulationResolver
                                                        └ Humanizer (correlated)
                                                                 │
                                                                 ▼
                                               SOURCE ENGINE (per string ×6)
                                               ├ SampleSource   ← primary
                                               └ ModelSource    ← fallback / bootstrap
                                               + NoiseEngine (performance noises)
                                               + ResonanceBank (sympathetic / open / body)
                                                                 │
                                                  ┌──────────────┴──────────────┐
                                                  ▼                             ▼
                                            PICKUP STAGE                  (Neck + Bridge
                                        (blend real N/B DI,               recorded DI)
                                         tone, output, load)
                                                  │
                                    ┌─────────────┼───────────────────┐
                                    ▼             ▼                   ▼
                              DI OUT BUS     AMP ENGINE          STEREO ENGINE
                              (raw, 2nd      (pick-dynamic,      (double-track /
                               output)        4 characters)       width)
                                                  │
                                                  ▼
                                            CAB ENGINE (IR: cab × mic × distance + room)
                                                  │
                                                  ▼
                                            GOA FX (attack-preserving)
                                                  │
                                                  ▼
                                             MAIN OUT BUS
```

**שני Output Buses:** `Main` (סטריאו, מעובד) ו-`DI` (מונו או סטריאו, נקי לגמרי אחרי ה-Pickup Stage). כך אפשר להקליט במקביל את הסאונד של DaliGTR ואת ה-DI ל-NAM, Neural DSP או חומרה.

---

## 2. Lookahead — ההחלטה החשובה ביותר ב-Brain

הדרישה "מה יהיה התו הבא" לא אפשרית ב-realtime טהור. הפתרון:

| מצב | Latency | שימוש |
|---|---|---|
| **Live** | 0 | נגינה חיה ממקלדת. Fingering חמדני (greedy) על בסיס התו הקודם בלבד |
| **Studio** (ברירת מחדל) | 60 ms | נגינה מתוך ה-DAW. הפלאגין מדווח `setLatencySamples`, ה-DAW מפצה, והכל מסונכרן |
| **Deep** | 120 ms | ריפים וסולואים מהירים. חלון ראייה רחב יותר ל-Fingering ולהחלטות legato |

ב-Studio וב-Deep ה-Brain רואה את כל התווים שבתוך החלון, ולכן יכול לתכנן fingering לכל המשפט, לזהות legato מראש, ולבחור slide לעומת re-pick לפי מה שבא אחר כך.

---

## 3. Guitar Brain

### 3.1 Data types

```cpp
struct NoteIntent {            // מה המשתמש ביקש
    int64 timeSamples; int pitch; float velocity; int64 durationSamples (if known);
    int  channel;              // 1–6 = forced string (guitar-MIDI convention), else auto
    ArticulationRequest req;   // from keyswitch / CC / sequencer
};

struct FretPosition { int string; int fret; };

enum class Technique { Pick, Hammer, Pull, SlideUp, SlideDown, SlideIn, SlideOut,
                       BendUp, PreBend, BendRelease, PalmMute, Staccato, DeadNote,
                       NaturalHarmonic, PinchHarmonic, Tremolo };

struct PerformanceEvent {      // מה הגיטריסט מבצע — הקלט היחיד של ה-Source
    int64 time; FretPosition pos; int pitch;
    Technique technique; PickDirection dir; float pickStrength;   // 0–1
    float pickPosition;        // neck..bridge
    TransitionInfo transition; // from pos/pitch, interval, speed (for legato/slide samples)
    RRKey rrKey;               // עבור RoundRobinSelector
    HumanOffsets human;        // timing, strength, cents — קטנים ומתואמים
};
```

### 3.2 FingeringSolver — Viterbi על חלון ה-Lookahead

במקום בחירה חמדנית לכל תו, המצב הוא (string, fret) לכל תו בחלון, ופונקציית עלות מינימלית לכל המסלול:

- **Hand travel:** תנועת יד מחוץ ל-box של 4 frets (עלות ריבועית, כי קפיצות גדולות "לא טבעיות").
- **Position preference:** Auto / Low / Mid / High / Custom.
- **String continuity:** בונוס להישאר על אותו מיתר כשהתווים צמודים (legato אפשרי).
- **String crossing:** עלות למעבר מיתרים, גבוהה יותר בדילוג על מיתר.
- **Timbre:** fretים גבוהים על מיתרים עבים, ומיתרים פתוחים באמצע סולו.
- **Polyphony:** תווים חופפים חייבים מיתרים שונים (אילוץ קשיח).
- **Speed:** ככל שהקצב מהיר, עלות תנועת היד גדלה.

`Realism` משפיע על משקלי העלות. ב-0% הבחירה פשוטה וצפויה, ב-100% זה fingering של גיטריסט מלא.
**Override:** MIDI channel 1–6 מכריח מיתר, ו-keyswitch "Position Lock" מקבע אזור.

### 3.3 HandModel
מחזיק את מיקום היד (index finger fret) ואת רוחב ה-box. מספק ל-Solver את עלות התזוזה, ל-NoiseEngine את אירועי ה-position shift (לרעשי fret), ול-GUI את מלבן היד.

### 3.4 PickingEngine
- מצבים: Alternate, Down-only, Economy, Tremolo, Sequencer-driven.
- **Grid-aware alternate:** כיוון הפריטה נגזר מהמיקום בגריד (downbeat = down) ולא רק מהתו הקודם, כמו גיטריסט אמיתי. אחרי הפסקה חוזרים ל-down.
- String crossing ב-Economy: הכיוון נבחר לפי כיוון המעבר.
- Pick strength = velocity ⊕ accent pattern ⊕ humanization.

### 3.5 LegatoEngine
החלטה לכל זוג תווים סמוכים, לפי: overlap, מרווח, מיתר, fret, מהירות, velocity ומיקום יד.

```
overlap && sameStringPossible && |interval| ≤ HammerRange && inHandBox → Hammer / Pull
overlap && sameStringPossible && |interval| ≤ SlideRange              → Slide (speed from IOI)
otherwise                                                              → Re-pick
velocity > RepickThreshold                                              → Re-pick (accent)
```
כל הספים ניתנים לכיוון, ו-Realism משנה אותם.

### 3.6 ArticulationResolver
משלב בקשות (keyswitch, CC, sequencer) עם החלטות ה-Brain, ובודק זמינות בספרייה. אם אין sample לטכניקה מסוימת, הוא מחליט על fallback מוגדר (למשל Pinch → Pick + drive boost), ולא מנגן שקט.

### 3.7 Humanizer
משתנה latent אחד (AR(1)) לכל "נגן", שמשפיע יחד על timing, עוצמה, מיקום פריטה, cents ובחירת RR. הטווחים קטנים: timing ±1–6 ms, velocity ±2–6, pitch ≤3 cents. **ב-Studio mode ההזזה יכולה להיות גם שלילית**, כי ה-lookahead מאפשר להקדים תווים.

---

## 4. Source Engine

### 4.1 Interface

```cpp
class IStringSource {             // one per string, monophonic like a real string
public:
    virtual void prepare (const EngineContext&) = 0;
    virtual void start (const PerformanceEvent&) = 0;       // pick, harmonic, dead note...
    virtual void transition (const PerformanceEvent&) = 0;  // hammer, pull, slide
    virtual void release (const PerformanceEvent&) = 0;
    virtual void setPitchMod (float semis) = 0;             // bend + vibrato + drift
    virtual void render (StereoPair out, DIPair diOut, int n) = 0;
};
```
`SampleSource` ו-`ModelSource` מממשים את אותו interface. ה-Brain לא יודע איזה מהם מנגן, ולכן כשמגיעה ספרייה אמיתית שום דבר מעל ה-Source לא משתנה.

### 4.2 SampleSource — מבנה

```
SampleLibrary (immutable, shared_ptr, swapped atomically)
 └─ Zone[]  { string, fretLo..fretHi, rootFret, velLayer, dir, technique, rr,
              transition{fromFret,toFret}, pickup channels, markers }
ZoneSelector      → scores zones for a PerformanceEvent
RoundRobinSelector→ history-aware, per (string, fret-zone, technique, dir, velLayer)
StringPlayer ×6   → up to 3 SampleVoices per string (current, crossfading-out, release/noise)
SampleVoice       → disk-streamed playback, high-quality resampler, pitch-mod
DiskStreamer      → background thread, preload head of each sample (≈ 64 KB) in RAM
```

**בחירת Zone (ZoneSelector):**
1. התאמה מדויקת לטכניקה (או fallback מה-Resolver).
2. אותו מיתר (חובה).
3. fret קרוב ל-rootFret: עדיף sample מאותו fret, ומותר resample עד ±1 fret (±1 semitone).
4. Velocity layer: בחירה ב-crossfade בין שתי שכבות סמוכות לפי pickStrength, ולא מעבר חד.
5. כיוון פריטה.

**RoundRobinSelector:** אף פעם לא אותו RR פעמיים ברצף לאותו מפתח, וזוכר את ה-RR האחרון לכל מפתח. כשיש מעט RR, מוסיף וריאציה עדינה מ-"שכן" (fret סמוך עם resample) במקום לחזור על אותו קובץ. כך נמנע Machine Gun גם בספרייה קטנה.

**Legato אמיתי על samples:**
- **עם sample מעבר** (hammer/pull/slide מוקלט מ-fret A ל-fret B): מנגנים את ה-transition ועושים crossfade קצר (5–15 ms) ל-sustain של היעד, מ-offset שמתאים לזמן שעבר מאז הפריטה (הדעיכה ממשיכה, לא מתחילה מחדש).
- **בלי sample מעבר:** attack-less sustain של היעד מאותו offset, crossfade של 3–8 ms, ושכבת noise מתאימה (tap או pluck) מה-NoiseEngine.

**Bends ו-Vibrato:** resampling רציף של ה-voice הנוכחי (עד 2 טונים נשמע טבעי בגיטרה). מעל 2 טונים, ואם יש בספרייה, bend מוקלט. העקומות מגיעות מה-BendEngine וה-VibratoEngine (סעיף 5).

**Resampler:** windowed-sinc 16-tap עם טבלה מוכנה מראש, איכות גבוהה בלי aliasing ב-bends.

### 4.2.1 ספריות בלי מידע על מיתרים
רוב ספריות ה-SFZ ממופות לפי pitch ולא לפי מיתר ו-fret. במקרה כזה:
- ה-Brain עדיין מחליט על מיתר ו-fret. ההחלטה קובעת legato, slides, רעשי יד, מיקום על הצוואר ו-voice allocation (תו אחד לכל מיתר).
- ה-ZoneSelector בוחר את ה-zone הקרוב ביותר ב-pitch, ואת ההבדל הקטן ב-timbre בין מיתרים ממלא **String Color**: פילטר עדין שמדמה מיתר עבה לעומת דק ו-fret גבוה לעומת נמוך, בעוצמה קטנה ומבוקרת.
- כשספרייה כן מכילה מידע על מיתרים (Standard Guitar חלקית), ה-Profile ממפה אותו וה-String Color נכבה.

### 4.3 ModelSource
מודל ה-waveguide הדו-מישורי מהגרסה הקודמת, מאחורי אותו interface. שלושה תפקידים:
1. **Fallback** לטכניקות חסרות בספרייה.
2. **ResonanceBank** (סעיף 7).
3. **Test library** ל-CI (רינדור קטן בפורמט DGL, כדי שהטסטים ירוצו בלי ספריות חיצוניות).

---

## 5. Expression Engines (משותפים לשני המקורות)

**BendEngine:** טווחים ¼, ½, 1, 1½, 2, 3, 4 טונים ו-12 semitones. מקור: Pitch Wheel, keyswitch Auto Bend, או sequencer. העקומה היא phases: attack → rise (spring עם ease-in/out) → overshoot קטן אופציונלי → hold → vibrato → release. יש Bend Speed ו-Curve, וכן Pre-bend, Release ו-Bend down.

**VibratoEngine (Finger Vibrato):** עקומה לכל מחזור: דחיפה מהירה, hold לפי Pressure, ושחרור איטי. Rate ו-Depth משתנים מעט בין מחזורים ובין תווים. Direction: Up (אצבע, pitch רק עולה) או Both (whammy). Mod Wheel שולט ב-amount, ו-Aftertouch ב-pressure/depth.

**PitchImperfections:** intonation לפי fret ומיתר (קבוע לכל "גיטרה"), glide קטן כלפי מטה אחרי פריטה חזקה (tension modulation), ו-drift איטי ≤ 2 cents.

---

## 6. Sample Library Format (DGL — Dali Guitar Library)

### 6.1 מבנה תיקיות
```
DaliGTR Library/
├── library.json                     ← manifest (schema below)
├── DI/
│   ├── S6/ ... S1/                  ← string folders
│   │   ├── sustain/   S6_F03_V2_D_RR1.wav
│   │   ├── palm/      S6_F03_V2_U_RR3.wav
│   │   ├── stacc/  dead/  harm/  pinch/
│   │   ├── legato/    S6_F03-F05_HAM_V2_RR1.wav   (from-to)
│   │   ├── slide/     S6_F03-F08_SLU_V2_RR1.wav
│   │   └── bend/      S6_F07_B200_V2_RR1.wav      (cents)
├── Noise/  pick/ fret/ string/ slide/ release/ mute/ scrape/
└── IR/     cab_4x12_V30/ SM57_cap_0cm.wav ...
```

### 6.2 קבצי אודיו
- WAV, 48 kHz, 24-bit.
- **שני ערוצים: Ch1 = Neck pickup DI, Ch2 = Bridge pickup DI**, מוקלטים בו-זמנית. זה מה שמאפשר Pickup Blend אמיתי ולא EQ (Middle = blend + comb מבוקר).
- pre-roll של 10 ms לפני ה-onset, מסומן כ-marker.
- Tail טבעי עד -70 dBFS, fade של 50 ms.
- ללא נורמליזציה (שומרים על היחסים הדינמיים בין שכבות).

### 6.3 library.json (תמצית)
```json
{
  "format": "DGL", "version": 1,
  "instrument": { "name": "Dali Strat DI", "tuning": [40,45,50,55,59,64], "frets": 22,
                  "channels": ["neck","bridge"] },
  "zones": [
    { "file": "DI/S6/sustain/S6_F03_V2_D_RR1.wav",
      "string": 6, "fret": 3, "technique": "sustain", "velocityLayer": 2,
      "dir": "down", "rr": 1, "pitchCents": -1.8, "onset": 480,
      "peakDb": -9.4, "loudnessLufs": -21.3 },
    { "file": "DI/S6/legato/S6_F03-F05_HAM_V2_RR1.wav",
      "string": 6, "fret": 5, "fromFret": 3, "technique": "hammer",
      "velocityLayer": 2, "rr": 1, "transitionAt": 22080 }
  ],
  "velocityLayers": [ {"max": 0.35}, {"max": 0.7}, {"max": 1.0} ],
  "noises": [ { "file": "Noise/fret/shift_up_03.wav", "type": "fretShift",
                "direction": "up", "distance": 3 } ]
}
```
את הקובץ `pitchCents` ואת ה-markers ממלא אוטומטית כלי `tools/library_builder` (זיהוי onset, מדידת pitch, loudness), כך שלא צריך לתייג ידנית.

### 6.4 תוכנית הקלטה (ספרייה v1 מינימלית ואיכותית)
| טכניקה | כיסוי | קבצים |
|---|---|---|
| Sustain | 6 מיתרים × כל fret שני (0–22 → 12) × 3 velocity × 2 כיוונים × 4 RR | 1,728 |
| Palm mute | 6 × 12 × 3 × 2 × 3 RR | 1,296 |
| Hammer / Pull | 6 × 12 × (±1, ±2 semitones) × 2 vel × 2 RR | 1,152 |
| Slides | 6 × 6 מיקומים × (±2, ±5, ±7) × 2 vel | 432 |
| Staccato / Dead / Harmonics / Pinch | כיסוי חלקי | ~600 |
| Noises | כל הסוגים | ~250 |
| **סה"כ** | | **~5,500 קבצים, ~3 GB** |

זה בערך 3–4 ימי הקלטה לגיטריסט מנוסה. הספרייה מתרחבת (כל fret, עוד RR, bends מוקלטים) בלי לגעת בקוד.

### 6.5 SFZ Importer ו-Library Profiles
ה-Importer רץ **בזמן בניית ה-Sound Set** (סעיף 0.3), וגם זמין בפלאגין להוספת ספריות SFZ בעתיד. התהליך:
1. **מפרסר את ה-SFZ** (תת-קבוצה של opcodes: `<control> <global> <group> <region>`, `#define`, `#include`, `sample`, `lokey/hikey/key`, `pitch_keycenter`, `lovel/hivel`, `seq_length/seq_position`, `lorand/hirand`, `sw_last/sw_lokey/sw_hikey`, `trigger=release`, `offset`, `tune`, `volume`, `loop_*`, `group/off_by`).
2. **מפעיל Library Profile** (`profiles/<library>.json`), קובץ קטן שמתרגם את ה-keyswitches וה-groups של הספרייה ל-Technique של DaliGTR. למשל ב-Standard Guitar: keyswitch X = `palm`, group Y = `hammer`.
3. **מייצר cache** בפורמט DGL (`library.json` בלבד, בלי להעתיק אודיו). הטעינות הבאות מהירות.
4. **מנתח את האודיו** פעם אחת (`library_builder`): onset אמיתי, pitch נמדד ו-loudness, כדי ש-velocity crossfades ו-RR יהיו אחידים.

FLAC נתמך ישירות (Standard Guitar מגיעה ב-FLAC).

---

## 7. Noise ו-Resonance

**NoiseEngine:** שכבות נפרדות, לכל אחת fader: Pick, Fret, String, Slide, Release, Muting, Amp. כל רעש **מופעל על ידי אירוע ביצוע** (position shift, string change, release, slide, mute) ולא מתוזמן אקראית. העוצמה נגזרת מהאירוע (מרחק תזוזה, מהירות, עוצמת פריטה). מקור: samples מה-`Noise/`, ו-fallback פרוצדורלי.

**ResonanceBank:** 6 waveguides מכוונים למיתרים הפתוחים (או ל-fret הנוכחי כשמיתר לחוץ ומושתק), מוזנים מאות הגוף. כולל Open ring (מיתר פתוח ממשיך אחרי שחרור), Sympathetic, Muted (resonance עם damping של יד), ו-Body (IR קצר של גוף הגיטרה). בקרה אחת: Resonance, וב-Advanced כל אחד בנפרד.

---

## 8. Pickup, DI, Amp, Cab, Stereo

**PickupStage:** Neck / Middle / Bridge + Blend. כשספרייה מכילה כמה ערוצי pickup (או pickup + mic כמו Shinyguitar) ה-Blend מתבצע בין ההקלטות. אחרת, pickup transform על ה-DI: comb לפי מיקום ה-pickup, תהודת סליל + כבל, ו-coil split, Tone (RC אמיתי של tone pot + capacitor), Output, ו-Load (ההשפעה של כבל/קלט על תהודת ה-pickup). **כאן נחתך ה-DI bus.**

**AmpEngine:** ארבעה characters: Clean, Edge, Crunch, High Gain. מבנה: input stage → 2–3 triode stages עם **bias drift דינמי** (פריטה חזקה מזיזה את ה-bias ומייצרת compression ו-harmonics שונים) → tone stack (FMV מתמטי) → power amp עם sag. Oversampling ×4 עד ×8. בהמשך: טעינת מודלים של NAM.

**CabEngine:** convolution (juce::dsp::Convolution, non-uniform partitioned). Cab type × Mic (57 / 421 / ribbon) × Position (cap → edge) × Distance. Blend בין שני מיקרופונים, Room, Low cut ו-High cut. Bypass = DI נטו.

**StereoEngine:** מצבים: Mono, Width, ו-**Double-track** (הביצוע השני מנוגן על ידי "גיטריסט שני" עם humanizer latent ו-RR נפרדים, מחולק שמאל/ימין). זו הדרך האמיתית שבה גיטרות נשמעות רחבות בגואה, ולא chorus.

---

## 9. Tone Explorer ו-Goa FX

### 9.1 Tone Explorer — למצוא את הגוון תוך כדי נגינה
- **Tone Snapshots:** לכל snapshot יש Guitar, Pickup, Amp, Cab ו-Goa. 8 סלוטים זמינים בלחיצה אחת, והמעבר ביניהם מתבצע בזמן נגינה בלי קליקים (crossfade של 50 ms).
- **Morph A↔B:** knob אחד שמעביר בהדרגה בין שני snapshots. כל הפרמטרים הרציפים עוברים ביניהם, והמעבר בין גיטרות (ספריות) נעשה ב-crossfade. מתאים גם לאוטומציה בתוך טראק.
- **Explore:** מציע וריאציה על הגוון הנוכחי בתוך גבולות טעם (למשל רק Pickup ו-Amp, או רק Goa), בלי לגעת במה שנעול.
- **Lock:** נעילה של שכבות. למשל "נעל את הגיטרה וה-Amp, תחקור רק FX".
- **Favorites:** שמירה עם שם ותגיות (Riff, Lead, Ambient).
- **Audition loop:** לולאת MIDI קצרה שמנגנת את הריף האחרון או ריף מובנה, כדי לשמוע שינויים בגוון בלי לנגן.

### 9.2 Goa FX

**שרשרת:** Tube Drive → Acid Filter (resonant, envelope follower + env generator מסונכרן, accent-aware) → Envelope Filter → Phaser → Flanger → Chorus → Psychedelic Delay (feedback path עם filter, drive ו-pitch) → Stereo Delay → Reverb / Space.

**Attack preservation:** transient detector על ה-dry. ה-wet של modulation, delay ו-reverb עובר ducking של 5–20 ms על כל attack, ככה שהפריטה נשארת חדה גם עם הרבה אפקטים.

**Presets:** Clean, Goa Clean, Crunch, Goa Crunch, Acid, Goa Lead, Goa Riff, Solo, High Gain, Tremolo. כל preset קובע גם הגדרות ב-Brain (picking mode, legato amount, position) ולא רק FX.

---

## 10. Riff & Solo Assistant — עוזר כתיבה לגיטרה

**העיקרון:** מלודיה של גיטרה נחשבת על הצוואר ולא על המקלדת. העוזר חושב בצורות: boxes, מיתרים, pedal tones ומיקום יד. מה שהוא מייצר ניתן לנגינה על גיטרה אמיתית, ונשמע גיטריסטי כשהוא עובר דרך ה-Brain.

### 10.1 Project Key
- **Root + Scale** גלובליים ל-DaliGTR ולעוזר (Minor, Harmonic Minor, Dorian, Phrygian, Phrygian Dominant, Major, Minor Pentatonic, Double Harmonic, Custom).
- **Key Detect:** מנגנים או מחזיקים כמה תווים או את הבס של הטראק, והעוזר מציע Root ו-Scale. (VST3 לא מקבל את הסולם מה-DAW, ולכן זו הדרך הנוחה.)

### 10.2 איך הוא בונה משפטים
- **Scale → Neck map:** כל הסולם מוצג על הצוואר ב-5 positions (pentatonic boxes) או ב-3 notes-per-string, והמשתמש בוחר position.
- **Riff Patterns (אוצר מילים גיטריסטי):** מוגדרים כצורות על הצוואר ולא כרשימות pitch:
  - **Pedal riff:** תו שורש על מיתר פתוח או נמוך לסירוגין עם מלודיה על מיתר גבוה. זה ה-DNA של ריפי גואה.
  - **Octave riff:** אוקטבות בצורת fret +2 ומיתר +2.
  - **String-skipping, Rolling 1/16, Call & Response, Chromatic approach.**
  - **Power-chord / double-stop riffs.**
- **Solo Licks:** licks מוגדרים יחסית ל-position, עם טכניקות (bend ¼–2 טונים, slide in/out, hammer/pull runs, vibrato בסוף משפט). העוזר משרשר licks לפי כיוון המשפט (עולה, יורד, קשת), אורך המשפט ומיקום היד.
- **פרמטרים:** Length (1–8 bars), Density, Complexity, Repetition (כמה המוטיב חוזר), Variation, Accent pattern, Octave, Position, Picking style, Legato amount, Rest amount.

### 10.3 עבודה איתו
- **Generate → Audition → Keep / Regenerate.** אפשר לנעול bars או תווים שאהבת ולייצר מחדש רק את השאר.
- **Variations:** מוטיב → 4 וריאציות (שינוי סיום, הזזת אוקטבה, קיצור, תשובה).
- **Tab View:** התוצאה מוצגת כטאב על הצוואר, כך שרואים איך "גיטריסט" מנגן אותה.
- **ייצוא:** גרירת MIDI ישירות ל-DAW, כולל keyswitches של טכניקות, או נגינה ישירה ב-DaliGTR בסנכרון לטמפו.
- **Humanize על הייצוא:** אופציונלי. בדרך כלל עדיף להשאיר את ה-humanize ל-Brain.

### 10.4 Picking Sequencer ו-ArpGuitarist
- **PickingSequencer:** grid של 1/8, 1/16, 1/32 ו-triplets. לכל step: on/off, accent, palm mute, direction, tie. חל על התווים המוחזקים ושולט ישירות ב-PickingEngine.
- **ArpGuitarist:** arpeggio על אקורד מוחזק, עם צורת אקורד אמיתית על הצוואר וסדר מיתרים.

### 10.5 מבנה ו-Ableton
- העוזר חי **רק בתוך DaliGTR** (Page 4), בלי פלאגין נוסף.
- **שתי דרכים להשתמש בתוצאה:**
  1. **Play:** DaliGTR מנגן את המשפט בעצמו, מסונכרן לטמפו ולמיקום של Ableton, עם כל ה-Brain.
  2. **Drag to Ableton:** גוררים את המשפט מהפלאגין ישר לטראק או לסלוט, והוא נוחת כ-MIDI clip. עובד ב-Live 11 וב-12 בלי תמיכה ב-MIDI output. ה-keyswitches של הטכניקות נכללים, כך שהקליפ מנוגן בדיוק כמו שנשמע.
- **Key:** Ableton 12 לא חושף ל-VST3 את ה-Scale Awareness שלו, ולכן Root ו-Scale נקבעים בפלאגין, עם Key Detect מתווים שמנגנים.
- **Latency:** מצב Studio (60 ms) מפוצה אוטומטית על ידי Ableton בזמן playback. להקלטה חיה ממקלדת עוברים למצב Live.
- כל ה-generators מייצרים **NoteIntents** שנכנסים ל-Input Stage, בדיוק כמו MIDI. הם לא מדברים עם ה-Sampler ישירות.

---

## 11. Class Map ו-Threads

```
Source/
├── Plugin/        DaliGTRProcessor, DaliGTREditor, Parameters, PresetManager, StateIO
├── Core/          EngineContext, EventQueue<T> (lock-free SPSC), RealtimePool, Smoothers
├── Brain/         InputStage, LookaheadBuffer, GuitarBrain, FingeringSolver, HandModel,
│                  PickingEngine, LegatoEngine, ArticulationResolver, Humanizer, Tuning
├── Expression/    BendEngine, VibratoEngine, PitchImperfections
├── Source/        IStringSource, StringRack(×6), SampleSource, ZoneSelector,
│                  RoundRobinSelector, SampleVoice, Resampler, DiskStreamer, ModelSource
├── Library/       LibraryManifest, SfzParser, LibraryProfile, LibraryLoader (bg), AudioAnalyzer, GuitarRack
├── Noise/         NoiseEngine, NoiseEvent, ResonanceBank
├── Tone/          PickupStage, AmpEngine (+ characters), CabEngine, StereoEngine
├── Goa/           GoaChain, AcidFilter, EnvFilter, Phaser, Flanger, Chorus,
│                  PsyDelay, StereoDelay, Space, TransientGuard
├── Tone/Explorer  ToneSnapshot, SnapshotBank, Morph, Explorer, Favorites
├── Assist/        ProjectKey, KeyDetect, NeckMap, RiffPatterns, LickLibrary, PhraseBuilder,
│                  VariationEngine, TabView, MidiExport, PickingSequencer, ArpGuitarist
├── UI/            DaliLookAndFeel, NeckView, PerformanceHUD, pages ×5
└── tools/         library_builder, render_bootstrap, audio_regression
```

**Threads:** Audio (הכל realtime), Loader (טעינת ספרייה ו-IR), Streamer (disk → ring buffers), UI (קורא state דרך atomic snapshots בלבד).

---

## 12. בדיקות אודיו — שער לכל Phase

1. **Unit tests** (CI, בלי JUCE): Brain, Solver, Resampler, Selectors.
2. **Render regression:** קובצי MIDI קבועים (`tests/phrases/*.mid`: ריף 1/16 Goa, סולו legato, bends, chugs, אקורדים) מרונדרים offline ב-CI ונשמרים כ-artifacts. השוואה מספרית לגרסה הקודמת: שינוי לא מכוון = כישלון.
3. **Machine-gun test:** 32 חזרות על אותו תו, ודאות ששום זוג רצוף לא זהה.
4. **Listening gate:** לכל Phase רשימת בדיקת האזנה, כולל A/B מול הקלטה אמיתית של אותו משפט. **Phase לא נסגר בלי אישור שלך באוזניים.**

---

## 13. Phases — מה נמסר בכל שלב

| # | Phase | יוצא מהשלב |
|---|---|---|
| 1 | Core | Processor, 2 output buses, EventQueue, Parameters, CI, render harness |
| 2 | Sample playback | Sound Set pipeline (CI), SFZ Importer + Profiles, SampleVoice + Resampler (FLAC/WAV) + streaming, first-run download. גיטרה ראשונה: SG |
| 3 | String/Fret | Lookahead, FingeringSolver (Viterbi), HandModel, Position modes, overrides |
| 4 | Picking + RR | PickingEngine, RoundRobinSelector, velocity crossfades |
| 5 | Legato + Slides | LegatoEngine, transitions, phase-matched crossfades |
| 6 | Bends + Vibrato | BendEngine, VibratoEngine, imperfections |
| 7 | Noise + Resonance | NoiseEngine (על רעשים מוקלטים), ResonanceBank, Continuation / Feedback |
| 8 | Pickup + DI | PickupStage, DI bus |
| 9 | Amp + Cab | AmpEngine ×4, CabEngine, Tone Snapshots + Morph |
| 10 | Goa | GoaChain, TransientGuard, presets |
| 11 | Riff & Solo Assistant | Project Key, Neck map, Riff patterns, Licks, Variations, Tab view, MIDI drag |
| 12 | GUI | 5 pages, NeckView, HUD |
| 13 | Optimization | CPU / RAM / streaming profiling |
| 14 | VST3 validation | pluginval + Steinberg validator + DAWs (Cubase, Ableton, FL, Reaper, Logic via AU later) |

---

## 14. Identity

- Product: **DaliGTR** · Manufacturer: **Dali Audio**
- Plugin code `DGTR`, manufacturer code `DliA`, bundle `com.daliaudio.daligtr`. זה קוד חדש, ולכן DAWs יזהו את DaliGTR כפלאגין נפרד מ-DaliGuitar.
- GUI: שחור / אפור כהה, סגול ניאון עדין כהדגשה בלבד, טיפוגרפיה נקייה, בלי עומס. לוגו "DaliGTR" גדול ומתחתיו "Dali Audio".

---

## 15. מה נשמר מהגרסה הקודמת

- Fretboard cost model: הופך לבסיס של ה-FingeringSolver.
- Waveguide הדו-מישורי: הופך ל-ResonanceBank ול-Continuation (sustain / feedback מוזן מההקלטה).
- Correlated Humanizer, Finger Vibrato, Bend spring: עוברים ל-Expression / Brain.
- CI + engine tests: מורחבים ל-render regression.
- GUI וה-GuitarChain הישנים: **לא נשמרים.**

## 16. סיכונים ידועים

| סיכון | השפעה | מענה |
|---|---|---|
| ספריות SFZ לא ממופות לפי מיתר | פחות גוון per-string | ה-Brain מחליט fingering, String Color עדין, ו-Profile לכל ספרייה |
| articulations שונות בין ספריות | חוסרים | ArticulationResolver + fallback מתועד לכל Profile |
| Lookahead מבלבל בנגינה חיה | latency | מצב Live נפרד |
| Legato על samples נשמע "מודבק" | ריאליזם | transition samples + crossfade מותאם phase, עם בדיקות A/B |
| גודל ספרייה | RAM / טעינה | disk streaming עם preload |
