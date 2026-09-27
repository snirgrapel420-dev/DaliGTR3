#pragma once
// DaliGTR engine, Phase 1: MIDI -> Brain v0 -> string rack (mono per string) -> recorded samples.
#include "Types.h"
#include "Brain.h"
#include "SamplePlayback.h"
#include <array>
#include <atomic>

namespace dgtr
{
struct EngineSettings
{
    float bendRangeSemis = 2.0f;
    int positionPref = Fretboard::Auto;
    float customFret = 7.0f;
    float releaseNoise = 0.6f;   // level of the recorded string-release samples
};

class GuitarEngine
{
public:
    // Live state for the GUI (written on the audio thread, read by the editor)
    struct Hud
    {
        std::atomic<int> note { -1 }, string { -1 }, fret { -1 }, velocity { 0 }, dir { 0 };
        std::array<std::atomic<int>, kNumStrings> stringFret {};
        std::array<std::atomic<float>, kNumStrings> stringLevel {};
        std::atomic<float> hand { 5.0f }, bend { 0.0f };
        Hud() { for (auto& f : stringFret) f = -1; for (auto& l : stringLevel) l = 0.0f; }
    };
    Hud hud;

    static constexpr int kMaxVoices = 32;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        brain.reset();
        allSoundOff();
    }

    // Called on the audio thread at a block boundary. The previous set is returned so the
    // caller can release it off the audio thread.
    GuitarSetPtr setGuitar (GuitarSetPtr g)
    {
        allSoundOff();
        auto old = std::move (guitar);
        guitar = std::move (g);
        selector.setGuitar (guitar.get());
        return old;
    }
    const GuitarSet* currentGuitar() const noexcept { return guitar.get(); }

    void setSettings (const EngineSettings& s)
    {
        settings = s;
        brain.positionPref = s.positionPref;
        brain.customFret = s.customFret;
    }

    void allSoundOff()
    {
        for (auto& v : voices) v = SampleVoice();
        for (auto& st : strings) st = StringState();
        heldKeys.fill (false);
    }

    // Renders n samples. Events must be sorted by offset.
    void process (const MidiEvent* events, int numEvents, float* L, float* R, int n)
    {
        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);
        int e = 0, pos = 0;
        while (pos < n)
        {
            while (e < numEvents && events[e].offset <= pos) handle (events[e++]);
            const int next = e < numEvents ? std::min (n, std::max (pos + 1, events[e].offset)) : n;
            for (auto& v : voices) v.render (L + pos, R + pos, next - pos);
            samplesElapsed += (uint64_t) (next - pos);
            pos = next;
        }
        while (e < numEvents) handle (events[e++]);
        publishHud();
    }

private:
    struct StringState
    {
        int voice = -1;      // index into voices
        int note = -1;       // sounding MIDI note
        int fret = -1;
        int key = -1;        // input key that owns the string (for note-off)
        int velocity = 0;
        float tuneShift = 0.0f;   // semitones from the recording to the target note (without bend)
    };

    void handle (const MidiEvent& ev)
    {
        switch (ev.type())
        {
            case 0x90:
                if (ev.data2 > 0) { noteOn (ev.data1, ev.data2); break; }
                noteOff (ev.data1);
                break;
            case 0x80: noteOff (ev.data1); break;
            case 0xE0:
            {
                const int v = (int) ev.data1 | ((int) ev.data2 << 7);
                bendSemis = (float) (v - 8192) / 8192.0f * settings.bendRangeSemis;
                for (auto& st : strings)
                    if (st.voice >= 0) voices[(size_t) st.voice].setPitch (st.tuneShift + bendSemis);
                break;
            }
            case 0xB0:
                if (ev.data1 == 120 || ev.data1 == 123) allNotesOff();
                break;
            default: break;
        }
    }

    unsigned busyMask() const
    {
        unsigned m = 0;
        for (int s = 0; s < kNumStrings; ++s) if (strings[(size_t) s].key >= 0) m |= 1u << s;
        return m;
    }

    void noteOn (int key, int vel)
    {
        if (guitar == nullptr) return;
        if (heldKeys[(size_t) key]) noteOff (key);

        int note = key;
        while (note > brain.board.highest()) note -= 12;
        while (note < brain.board.lowest())  note += 12;

        const auto d = brain.onNote (note, (double) samplesElapsed / sr, busyMask());
        if (! d.pos.valid()) return;

        const int slot = selector.findNoteSlot (note, vel);
        const int zi = selector.pickZone (slot);
        if (zi < 0) return;
        const auto& z = guitar->zones[(size_t) zi];
        const auto& sample = guitar->samples[(size_t) z.sample];

        auto& st = strings[(size_t) d.pos.string];
        if (st.voice >= 0) voices[(size_t) st.voice].fadeOutFast (6.0f);   // the pick stops the ringing string

        const int vi = allocateVoice();
        auto& v = voices[(size_t) vi];
        const float shift = (float) (note - z.keyCenter) + z.tuneCents * 0.01f;
        // within-layer dynamics: the recorded layer sets the timbre, velocity fine-tunes the level
        const float layerPos = z.hiVel > z.loVel ? (float) (vel - z.loVel) / (float) (z.hiVel - z.loVel) : 1.0f;
        const float gain = dbToGain (z.gainDb) * (0.62f + 0.38f * clampf (layerPos, 0.0f, 1.0f));
        v.start (sample, sr, shift + bendSemis, gain);
        v.ownerString = d.pos.string;
        v.age = ++ageCounter;

        st.voice = vi; st.note = note; st.fret = d.pos.fret; st.key = key; st.velocity = vel; st.tuneShift = shift;
        heldKeys[(size_t) key] = true;

        hud.note = note; hud.string = d.pos.string; hud.fret = d.pos.fret; hud.velocity = vel;
        hud.dir = d.dir == PickDir::Down ? 0 : 1;
    }

    void noteOff (int key)
    {
        if (! heldKeys[(size_t) key]) return;
        heldKeys[(size_t) key] = false;
        for (int s = 0; s < kNumStrings; ++s)
        {
            auto& st = strings[(size_t) s];
            if (st.key != key) continue;
            if (st.voice >= 0) voices[(size_t) st.voice].release (guitar != nullptr ? guitar->releaseSec : 0.1f);
            triggerRelease (st.note, st.velocity);
            st = StringState();
        }
    }

    void allNotesOff()
    {
        for (int k = 0; k < 128; ++k) if (heldKeys[(size_t) k]) noteOff (k);
    }

    // Recorded "finger leaves the string" sound
    void triggerRelease (int note, int vel)
    {
        if (guitar == nullptr || settings.releaseNoise <= 0.0f) return;
        const int slot = selector.findReleaseSlot (note, vel);
        if (slot < 0) return;
        const int zi = selector.pickZone (slot);
        if (zi < 0) return;
        const auto& z = guitar->zones[(size_t) zi];
        auto& v = voices[(size_t) allocateVoice()];
        v.start (guitar->samples[(size_t) z.sample], sr, (float) (note - z.keyCenter) + z.tuneCents * 0.01f,
                 dbToGain (z.gainDb) * settings.releaseNoise * (0.5f + 0.5f * (float) vel / 127.0f), 0.0f);
        v.ownerString = -1;
        v.age = ++ageCounter;
    }

    int allocateVoice()
    {
        for (int i = 0; i < kMaxVoices; ++i)
            if (! voices[(size_t) i].isActive())
            {
                for (auto& st : strings) if (st.voice == i) st.voice = -1;   // string whose note already died
                return i;
            }
        // steal the oldest voice that is not a held string
        int best = 0; uint32_t bestAge = UINT32_MAX;
        for (int i = 0; i < kMaxVoices; ++i)
        {
            const bool held = voices[(size_t) i].ownerString >= 0 && strings[(size_t) voices[(size_t) i].ownerString].voice == i;
            const uint32_t a = voices[(size_t) i].age + (held ? 0x40000000u : 0u);
            if (a < bestAge) { bestAge = a; best = i; }
        }
        for (auto& st : strings) if (st.voice == best) st.voice = -1;
        voices[(size_t) best] = SampleVoice();
        return best;
    }

    void publishHud()
    {
        for (int s = 0; s < kNumStrings; ++s)
        {
            const auto& st = strings[(size_t) s];
            const bool on = st.voice >= 0 && voices[(size_t) st.voice].isActive();
            hud.stringFret[(size_t) s] = on ? st.fret : -1;
            hud.stringLevel[(size_t) s] = on ? voices[(size_t) st.voice].currentLevel() : 0.0f;
        }
        hud.hand = brain.handPosition();
        hud.bend = bendSemis;
    }

    double sr = 48000.0;
    uint64_t samplesElapsed = 0;
    uint32_t ageCounter = 0;
    GuitarSetPtr guitar;
    EngineSettings settings;
    BrainV0 brain;
    ZoneSelector selector;
    std::array<SampleVoice, kMaxVoices> voices {};
    std::array<StringState, kNumStrings> strings {};
    std::array<bool, 128> heldKeys {};
    float bendSemis = 0.0f;
};
} // namespace dgtr
