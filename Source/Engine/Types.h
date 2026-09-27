#pragma once
// DaliGTR engine core types. Plain C++17, no JUCE: the whole engine is unit-testable in CI.
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <map>
#include <tuple>

namespace dgtr
{
constexpr float kPi = 3.14159265358979f;

inline float clampf (float x, float a, float b) { return std::min (std::max (x, a), b); }
inline float dbToGain (float db)                { return std::pow (10.0f, db / 20.0f); }

struct FastRandom
{
    uint32_t s = 0x12345678u;
    void seed (uint32_t v)  { s = v != 0 ? v : 0x9e3779b9u; }
    uint32_t nextU()        { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni()             { return (float) (nextU() >> 8) * (1.0f / 16777216.0f); }
    float bi()              { return uni() * 2.0f - 1.0f; }
    int below (int n)       { return n <= 1 ? 0 : (int) (uni() * (float) n) % n; }
};

//==============================================================================
// Sound Set data (filled by the JUCE loader from guitar.json, or by tests directly)

enum class ZoneKind : uint8_t { Note, Release, Noise };

struct SampleData
{
    std::vector<float> left, right;   // right empty = mono
    double sampleRate = 44100.0;
    int onset = 0;                    // first sample of the real attack (measured by the builder)
    int numFrames() const noexcept    { return (int) left.size(); }
    bool isStereo() const noexcept    { return ! right.empty(); }
};

struct Zone
{
    ZoneKind kind = ZoneKind::Note;
    int sample = -1;
    int loKey = 0, hiKey = 127, keyCenter = 60;
    int loVel = 1, hiVel = 127;
    int slot = 0;                 // zones with the same slot are round-robin alternatives
    float tuneCents = 0.0f;       // correction measured by the builder (keeps every sample in tune)
    float gainDb = 0.0f;
    std::string noiseType;        // for Noise zones: "fingering", "muted", "pickrest", ...
};

struct Slot
{
    ZoneKind kind = ZoneKind::Note;
    int loKey = 0, hiKey = 127, keyCenter = 60, loVel = 1, hiVel = 127;
    std::string noiseType;
    std::vector<int> zones;       // indices into GuitarSet::zones
};

struct GuitarSet
{
    std::string id, name;
    std::vector<SampleData> samples;
    std::vector<Zone> zones;
    std::vector<Slot> slots;      // built by finalize()
    float releaseSec = 0.1f;

    // Group zones into round-robin slots. Must be called after zones are filled.
    void finalize()
    {
        slots.clear();
        std::map<std::tuple<int, int>, int> index;   // (kind, slot id) -> slots[]
        for (int z = 0; z < (int) zones.size(); ++z)
        {
            const auto& zn = zones[(size_t) z];
            if (zn.sample < 0 || zn.sample >= (int) samples.size()) continue;
            const auto key = std::make_tuple ((int) zn.kind, zn.slot);
            auto it = index.find (key);
            if (it == index.end())
            {
                Slot s;
                s.kind = zn.kind; s.loKey = zn.loKey; s.hiKey = zn.hiKey; s.keyCenter = zn.keyCenter;
                s.loVel = zn.loVel; s.hiVel = zn.hiVel; s.noiseType = zn.noiseType;
                slots.push_back (s);
                it = index.emplace (key, (int) slots.size() - 1).first;
            }
            slots[(size_t) it->second].zones.push_back (z);
        }
    }

    int lowestNoteKey() const
    {
        int k = 127;
        for (auto& s : slots) if (s.kind == ZoneKind::Note) k = std::min (k, s.loKey);
        return k;
    }
    int highestNoteKey() const
    {
        int k = 0;
        for (auto& s : slots) if (s.kind == ZoneKind::Note) k = std::max (k, s.hiKey);
        return k;
    }
};

using GuitarSetPtr = std::shared_ptr<const GuitarSet>;

//==============================================================================
// Minimal MIDI event (the processor converts juce::MidiBuffer into these)
struct MidiEvent
{
    int offset = 0;
    uint8_t status = 0, data1 = 0, data2 = 0;
    int type() const noexcept    { return status & 0xF0; }
    int channel() const noexcept { return (status & 0x0F) + 1; }
};

enum class PickDir : uint8_t { Down, Up };
} // namespace dgtr
