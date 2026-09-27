#pragma once
// Sample playback: zone selection, history-aware round robin, and the resampling voice.
#include "Types.h"

namespace dgtr
{
//==============================================================================
class ZoneSelector
{
public:
    void setGuitar (const GuitarSet* g)
    {
        guitar = g;
        lastPick.assign (g != nullptr ? g->slots.size() : 0, -1);
        rng.seed (0xD417u);
    }

    // Note slot for a key and velocity. If no slot covers the key (edge of range),
    // the nearest recorded key centre is used and the voice resamples.
    int findNoteSlot (int key, int vel) const { return find (ZoneKind::Note, key, vel); }
    int findReleaseSlot (int key, int vel) const { return find (ZoneKind::Release, key, vel); }

    int findNoiseSlot (const std::string& type) const
    {
        if (guitar == nullptr) return -1;
        for (int i = 0; i < (int) guitar->slots.size(); ++i)
            if (guitar->slots[(size_t) i].kind == ZoneKind::Noise && guitar->slots[(size_t) i].noiseType == type) return i;
        return -1;
    }

    // Round robin: never the same recording twice in a row for a slot.
    int pickZone (int slot)
    {
        if (guitar == nullptr || slot < 0) return -1;
        const auto& members = guitar->slots[(size_t) slot].zones;
        if (members.empty()) return -1;
        int& last = lastPick[(size_t) slot];
        int idx = rng.below ((int) members.size());
        if (members.size() > 1 && idx == last) idx = (idx + 1 + rng.below ((int) members.size() - 1)) % (int) members.size();
        last = idx;
        return members[(size_t) idx];
    }

private:
    int find (ZoneKind kind, int key, int vel) const
    {
        if (guitar == nullptr) return -1;
        int best = -1, bestDist = 1 << 30;
        for (int i = 0; i < (int) guitar->slots.size(); ++i)
        {
            const auto& s = guitar->slots[(size_t) i];
            if (s.kind != kind) continue;
            const bool velOk = vel >= s.loVel && vel <= s.hiVel;
            const bool keyOk = key >= s.loKey && key <= s.hiKey;
            // exact match wins; otherwise distance in key centres, velocity mismatch penalised
            const int dist = (keyOk ? 0 : 1000 + std::abs (key - s.keyCenter) * 10) + (velOk ? 0 : 100000);
            if (dist < bestDist) { bestDist = dist; best = i; }
        }
        return best;
    }

    const GuitarSet* guitar = nullptr;
    std::vector<int> lastPick;
    FastRandom rng;
};

//==============================================================================
// Plays one recording with continuously variable pitch (for bends, vibrato, slides later).
class SampleVoice
{
public:
    bool isActive() const noexcept { return data != nullptr; }
    int  ownerString = -1;          // -1 = not a string note (release / noise)
    uint32_t age = 0;

    void start (const SampleData& d, double engineRate, float semitoneShift, float gain, float prerollMs = 1.5f)
    {
        data = &d;
        engineSR = engineRate;
        const int preroll = (int) (prerollMs * 0.001 * d.sampleRate);
        pos = (double) std::max (0, d.onset - preroll);
        setPitch (semitoneShift);
        amp = gain;
        env = 1.0f;
        envStep = 0.0f;
        fadeIn = 1.0f;
        fadeInStep = 0.0f;
        if (pos > 0.0) { fadeIn = 0.0f; fadeInStep = 1.0f / (float) std::max (1.0, 0.0005 * engineSR); }   // 0.5 ms de-click
    }

    // semitone shift relative to the recording (includes key offset, tuning fix, bend...)
    void setPitch (float semis)
    {
        if (data == nullptr) return;
        increment = std::exp2 ((double) semis / 12.0) * data->sampleRate / engineSR;
    }

    void release (float seconds)
    {
        if (data == nullptr) return;
        envStep = -1.0f / (float) std::max (1.0, (double) seconds * engineSR);
    }

    void fadeOutFast (float ms = 6.0f) { release (ms * 0.001f); }

    void render (float* L, float* R, int n)
    {
        if (data == nullptr) return;
        const float* l = data->left.data();
        const float* r = data->isStereo() ? data->right.data() : l;
        const int last = data->numFrames() - 3;

        for (int i = 0; i < n; ++i)
        {
            const int ip = (int) pos;
            if (ip >= last || env <= 0.0f) { data = nullptr; ownerString = -1; return; }
            const float f = (float) (pos - (double) ip);
            const float g = amp * env * fadeIn;
            L[i] += g * hermite (l, ip, f);
            R[i] += g * hermite (r, ip, f);
            pos += increment;
            env += envStep;
            if (fadeIn < 1.0f) fadeIn = std::min (1.0f, fadeIn + fadeInStep);
        }
    }

    float currentLevel() const noexcept { return amp * std::max (0.0f, env); }

private:
    static float hermite (const float* x, int i, float f) noexcept
    {
        const float xm1 = x[i > 0 ? i - 1 : 0], x0 = x[i], x1 = x[i + 1], x2 = x[i + 2];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    const SampleData* data = nullptr;
    double pos = 0.0, increment = 1.0, engineSR = 48000.0;
    float amp = 1.0f, env = 1.0f, envStep = 0.0f, fadeIn = 1.0f, fadeInStep = 0.0f;
};
} // namespace dgtr
