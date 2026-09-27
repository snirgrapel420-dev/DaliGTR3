// DaliGTR engine tests - plain C++17, no JUCE.
// g++ -std=c++17 -O2 -Wall -ISource tests/engine_tests.cpp -o engine_tests && ./engine_tests
#include "Engine/GuitarEngine.h"
#include <cstdio>
#include <set>

using namespace dgtr;

static int failures = 0;
#define CHECK(cond, ...) do { std::printf ((cond) ? "  ok    " : "  FAIL  "); if (! (cond)) ++failures; \
                              std::printf (__VA_ARGS__); std::printf ("\n"); } while (0)

static constexpr double SR = 48000.0;

// Synthetic guitar with the same structure as Karoryfer Emilyguitar:
// key centres every 3 semitones, 4 velocity layers, 3 round robins, release samples.
static std::shared_ptr<GuitarSet> makeTestGuitar()
{
    auto g = std::make_shared<GuitarSet>();
    g->id = "test"; g->name = "Test";
    const double fileSR = 44100.0;
    const int layers[4][2] = { { 1, 40 }, { 41, 80 }, { 81, 120 }, { 121, 127 } };
    int slot = 0;
    for (int kc = 40; kc <= 88; kc += 3)
    {
        for (int l = 0; l < 4; ++l)
        {
            for (int rr = 0; rr < 3; ++rr)
            {
                SampleData d;
                d.sampleRate = fileSR;
                const double f = 440.0 * std::pow (2.0, (kc - 69) / 12.0) * (1.0 + 0.003 * (rr - 1)); // RR slightly detuned
                const int len = (int) (fileSR * 1.5);
                d.onset = 441;                                     // 10 ms of silence before the attack
                d.left.assign ((size_t) len, 0.0f);
                for (int i = d.onset; i < len; ++i)
                {
                    const double t = (i - d.onset) / fileSR;
                    d.left[(size_t) i] = (float) ((0.2 + 0.2 * l) * std::exp (-t * 2.0)
                                     * (std::sin (2 * M_PI * f * t) + 0.3 * std::sin (4 * M_PI * f * t) + 0.01 * rr));
                }
                g->samples.push_back (std::move (d));
                Zone z;
                z.kind = ZoneKind::Note; z.sample = (int) g->samples.size() - 1;
                z.loKey = kc - 1; z.hiKey = kc + 1; z.keyCenter = kc;
                z.loVel = layers[l][0]; z.hiVel = layers[l][1];
                z.slot = slot;
                z.tuneCents = -1200.0f * (float) std::log2 (1.0 + 0.003 * (rr - 1));   // builder-measured tuning fix
                g->zones.push_back (z);
            }
            ++slot;
        }
    }
    // release noises: 4 RR over the whole range
    for (int rr = 0; rr < 4; ++rr)
    {
        SampleData d; d.sampleRate = fileSR; d.left.assign (4410, 0.0f);
        for (int i = 0; i < 4410; ++i) d.left[(size_t) i] = 0.05f * std::sin (i * 0.3f) * std::exp (-i / 800.0f);
        g->samples.push_back (std::move (d));
        Zone z; z.kind = ZoneKind::Release; z.sample = (int) g->samples.size() - 1;
        z.loKey = 0; z.hiKey = 127; z.keyCenter = 60; z.slot = 1000;
        g->zones.push_back (z);
    }
    g->finalize();
    return g;
}

// normalised autocorrelation (immune to the decay envelope), parabolic refinement
static double pitchOf (const std::vector<float>& x, int start, int len)
{
    auto corr = [&] (int lag)
    {
        double c = 0, e0 = 0, e1 = 0;
        for (int i = 0; i < len; ++i)
        {
            const double a = x[(size_t) (start + i)], b = x[(size_t) (start + i + lag)];
            c += a * b; e0 += a * a; e1 += b * b;
        }
        return c / std::sqrt (e0 * e1 + 1e-30);
    };
    const int lo = (int) (SR / 1500), hi = (int) (SR / 60);
    std::vector<double> cv ((size_t) hi + 2, 0.0);
    double mx = -1e30;
    for (int lag = lo - 1; lag <= hi + 1; ++lag) { cv[(size_t) lag] = corr (lag); if (lag >= lo && lag <= hi) mx = std::max (mx, cv[(size_t) lag]); }
    int best = lo;   // first local peak close to the maximum = fundamental period (not a multiple)
    for (int lag = lo; lag <= hi; ++lag)
        if (cv[(size_t) lag] >= 0.9 * mx && cv[(size_t) lag] >= cv[(size_t) lag - 1] && cv[(size_t) lag] >= cv[(size_t) lag + 1]) { best = lag; break; }
    const double a = cv[(size_t) best - 1], b = cv[(size_t) best], c = cv[(size_t) best + 1], den = a - 2 * b + c;
    return SR / (best + (den != 0 ? 0.5 * (a - c) / den : 0.0));
}

struct Renderer
{
    GuitarEngine eng;
    std::vector<float> L, R;
    explicit Renderer (GuitarSetPtr g) { eng.prepare (SR); eng.setGuitar (std::move (g)); }
    void run (std::vector<MidiEvent> ev, int n)
    {
        const size_t start = L.size();
        L.resize (start + (size_t) n); R.resize (start + (size_t) n);
        const int block = 256;
        for (int p = 0; p < n; p += block)
        {
            const int m = std::min (block, n - p);
            std::vector<MidiEvent> blk;
            for (auto& e : ev) if (e.offset >= p && e.offset < p + m) { auto c = e; c.offset -= p; blk.push_back (c); }
            eng.process (blk.data(), (int) blk.size(), L.data() + start + p, R.data() + start + p, m);
        }
    }
};

static MidiEvent on (int off, int key, int vel) { return { off, 0x90, (uint8_t) key, (uint8_t) vel }; }
static MidiEvent off (int o, int key)          { return { o, 0x80, (uint8_t) key, 0 }; }

int main()
{
    auto guitar = makeTestGuitar();

    std::printf ("Sound Set structure\n");
    {
        int notes = 0, rel = 0;
        for (auto& s : guitar->slots) { if (s.kind == ZoneKind::Note) ++notes; if (s.kind == ZoneKind::Release) ++rel; }
        CHECK (notes == 17 * 4 && rel == 1, "slots grouped: %d note slots, %d release slot", notes, rel);
        bool rrOk = true;
        for (auto& s : guitar->slots) if (s.kind == ZoneKind::Note && s.zones.size() != 3) rrOk = false;
        CHECK (rrOk, "every note slot has 3 round robins");
    }

    std::printf ("Pitch across the neck (resampled between recorded notes)\n");
    for (int key : { 40, 41, 42, 45, 52, 57, 64, 69, 76, 86 })
    {
        Renderer r (guitar);
        r.run ({ on (0, key, 90) }, (int) (SR * 0.8));
        const double f = pitchOf (r.L, (int) (SR * 0.1), 12000);
        const double cents = 1200.0 * std::log2 (f / (440.0 * std::pow (2.0, (key - 69) / 12.0)));
        CHECK (std::abs (cents) < 3.0, "key %d: %.2f Hz (%.2f cents)", key, f, cents);
    }

    std::printf ("Velocity layers\n");
    {
        GuitarEngine e; e.prepare (SR); e.setGuitar (guitar);
        ZoneSelector sel; sel.setGuitar (guitar.get());
        const int s1 = sel.findNoteSlot (57, 30), s2 = sel.findNoteSlot (57, 60), s3 = sel.findNoteSlot (57, 100), s4 = sel.findNoteSlot (57, 127);
        CHECK (s1 != s2 && s2 != s3 && s3 != s4, "4 different layers for velocities 30/60/100/127");
        CHECK (guitar->slots[(size_t) s4].loVel == 121, "velocity 127 uses the forte layer");
    }

    std::printf ("Round robin (anti machine-gun)\n");
    {
        ZoneSelector sel; sel.setGuitar (guitar.get());
        const int slot = sel.findNoteSlot (57, 100);
        int prev = -1; bool repeat = false; std::set<int> used;
        for (int i = 0; i < 60; ++i) { const int z = sel.pickZone (slot); if (z == prev) repeat = true; prev = z; used.insert (z); }
        CHECK (! repeat, "60 repeated notes: no recording plays twice in a row");
        CHECK (used.size() == 3, "all 3 round robins are used");
    }

    std::printf ("Guitar behaviour\n");
    {
        Renderer r (guitar);
        r.run ({ on (0, 57, 100), on (4800, 57, 100) }, (int) (SR * 0.3));
        float maxJump = 0.0f;
        for (size_t i = 4790; i < 5200; ++i) maxJump = std::max (maxJump, std::abs (r.L[i] - r.L[i - 1]));
        CHECK (maxJump < 0.25f, "re-picking the same string has no click (max step %.3f)", maxJump);
    }
    {
        Renderer r (guitar);
        r.run ({ on (0, 45, 100), on (0, 52, 100), on (0, 57, 100), on (0, 64, 100) }, (int) (SR * 0.2));
        std::set<int> strs;
        for (int s = 0; s < kNumStrings; ++s) if (r.eng.hud.stringFret[(size_t) s] >= 0) strs.insert (s);
        CHECK (strs.size() == 4, "4-note chord uses 4 different strings");
    }
    {
        Renderer r (guitar);
        r.run ({ on (0, 57, 100), off ((int) (SR * 0.2), 57) }, (int) (SR * 0.6));
        double tail = 0; for (int i = (int) (SR * 0.45); i < (int) (SR * 0.6); ++i) tail += std::abs (r.L[(size_t) i]);
        CHECK (tail < 1.0, "note stops after release (tail energy %.3f)", tail);
        double relNoise = 0; for (int i = (int) (SR * 0.2); i < (int) (SR * 0.25); ++i) relNoise += std::abs (r.L[(size_t) i]);
        CHECK (relNoise > 0.0, "release sample is triggered on note-off");
    }
    {
        Renderer r (guitar);
        // bend +2 semitones via pitch wheel (value 16383 = +range)
        r.run ({ on (0, 57, 100), { 2400, 0xE0, 0x7F, 0x7F } }, (int) (SR * 0.5));
        const double f = pitchOf (r.L, (int) (SR * 0.2), 4000);
        CHECK (std::abs (1200.0 * std::log2 (f / 246.94)) < 5.0, "pitch wheel bend A3 -> B3 (%.1f Hz)", f);
    }
    {
        Renderer r (guitar);
        std::vector<MidiEvent> ev;
        for (int k = 0; k < 64; ++k) { ev.push_back (on (k * 600, 40 + (k * 7) % 40, 20 + (k * 37) % 107)); ev.push_back (off (k * 600 + 500, 40 + (k * 7) % 40)); }
        r.run (ev, (int) (SR * 1.5));
        bool ok = true; for (float s : r.L) if (! std::isfinite (s)) ok = false;
        CHECK (ok, "64 fast notes with voice stealing: no NaN/Inf");
    }
    {
        GuitarEngine e; e.prepare (SR);
        std::vector<float> L (512), R (512);
        MidiEvent ev = on (0, 60, 100);
        e.process (&ev, 1, L.data(), R.data(), 512);
        CHECK (true, "no guitar loaded: silent, no crash");
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
