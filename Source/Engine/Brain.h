#pragma once
// Guitar Brain v0 (Phase 1): string/fret choice + alternate picking.
// Phase 3 replaces the greedy choice with the lookahead Viterbi FingeringSolver.
#include "Types.h"

namespace dgtr
{
constexpr int kNumStrings = 6;
constexpr int kNumFrets = 22;

struct FretPos
{
    int string = -1;   // 0 = low E (string 6) .. 5 = high E (string 1)
    int fret = -1;
    bool valid() const noexcept { return string >= 0; }
};

class Fretboard
{
public:
    enum Pref { Auto = 0, Low, Mid, High, Custom };

    void setTuning (const int (&t)[kNumStrings]) { for (int s = 0; s < kNumStrings; ++s) open[s] = t[s]; }
    int openNote (int s) const noexcept  { return open[s]; }
    int lowest() const noexcept          { return open[0]; }
    int highest() const noexcept         { return open[kNumStrings - 1] + kNumFrets; }

    int fretFor (int s, int note) const noexcept
    {
        const int f = note - open[s];
        return (f >= 0 && f <= kNumFrets) ? f : -1;
    }

    FretPos choose (int note, float handPos, int pref, float customFret, unsigned busyMask, int preferString) const
    {
        float target = handPos;
        if (pref == Low) target = 2.0f; else if (pref == Mid) target = 7.0f;
        else if (pref == High) target = 14.0f; else if (pref == Custom) target = customFret;

        FretPos best;
        float bestCost = 1.0e9f;
        for (int s = 0; s < kNumStrings; ++s)
        {
            const int f = fretFor (s, note);
            if (f < 0) continue;
            float cost = (busyMask & (1u << s)) ? 1000.0f : 0.0f;
            if (f == 0) cost += (target > 9.0f ? 5.0f : 1.2f) + (pref == High ? 4.0f : 0.0f);
            else
            {
                const float d = std::abs ((float) f - target);
                cost += d <= 2.5f ? d * 0.4f : 1.0f + (d - 2.5f) * 1.1f;
                cost += (float) f * 0.04f;
                if (s <= 1 && f > 12) cost += (float) (f - 12) * 0.35f;
            }
            if (pref == Auto) cost += (float) f * 0.08f;
            if (s == preferString) cost -= 0.8f;
            if (cost < bestCost) { bestCost = cost; best = { s, f }; }
        }
        return best;
    }

private:
    int open[kNumStrings] { 40, 45, 50, 55, 59, 64 };
};

struct NoteDecision
{
    FretPos pos;
    PickDir dir = PickDir::Down;
};

class BrainV0
{
public:
    Fretboard board;
    int positionPref = Fretboard::Auto;
    float customFret = 7.0f;

    void reset() { hand = 5.0f; lastString = -1; lastDown = false; lastTime = -1.0e9; }

    NoteDecision onNote (int note, double timeSec, unsigned busyMask)
    {
        NoteDecision d;
        const int prefer = (lastString >= 0 && ! (busyMask & (1u << lastString))) ? lastString : -1;
        d.pos = board.choose (note, hand, positionPref, customFret, busyMask, prefer);
        if (! d.pos.valid()) return d;

        if (d.pos.fret > 0)
        {
            const float delta = (float) d.pos.fret - hand;
            hand = std::abs (delta) > 2.5f ? (float) d.pos.fret : hand + 0.25f * delta;
        }
        // alternate picking; a new phrase (gap > 300 ms) starts with a downstroke
        const bool down = (timeSec - lastTime) > 0.3 ? true : ! lastDown;
        d.dir = down ? PickDir::Down : PickDir::Up;
        lastDown = down;
        lastTime = timeSec;
        lastString = d.pos.string;
        return d;
    }

    float handPosition() const noexcept { return hand; }

private:
    float hand = 5.0f;
    int lastString = -1;
    bool lastDown = false;
    double lastTime = -1.0e9;
};
} // namespace dgtr
