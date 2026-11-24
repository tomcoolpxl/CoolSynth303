#include "Sequencer.h"

#include <algorithm>

namespace cs303
{
    PlayBank::PlayBank(const Project& p)
    {
        for (size_t i = 0; i < bank.size(); ++i)
            for (size_t v = 0; v < 2; ++v)
                bank[i][v] = PlayPattern(p.bank[i][v]);
        chainLength = std::clamp(static_cast<int>(p.chain.size()), 1, maxChain);
        for (int i = 0; i < chainLength; ++i)
            chain[static_cast<size_t>(i)] = p.chain.empty() ? ChainEntry {} : p.chain[static_cast<size_t>(i)];
        chainOn = p.chainOn;
        loop = p.loop;
    }

    double noteVolts(int key) noexcept
    {
        // the A of the middle octave (key 21) is 2.75 V, the service notes' 110 Hz; the ladder's top is 5.33 V
        return std::clamp(1.0 + key / 12.0, 0.0, 5.25);
    }

    void Sequencer::requestPattern(int index, bool variationB) noexcept
    {
        index = std::clamp(index, 0, Project::patterns - 1);
        if (! startedAny)
        {
            hearPattern = index;
            hearB = variationB;
            queued = -1;
            pos = first();
        }
        else if (index == pos.pattern && variationB == pos.variationB && queued < 0)
            return;
        else
        {
            queued = index;
            queuedB = variationB;
        }
    }

    void Sequencer::reset() noexcept
    {
        if (queued >= 0)
        {
            hearPattern = queued;
            hearB = queuedB;
            queued = -1;
        }
        startedAny = false;
        lastSixteenth = -1;
        pos = first();
        now = {};
    }

    Sequencer::Position Sequencer::first() const noexcept
    {
        Position p;
        if (bank.chainOn)
        {
            const auto& e = bank.chain[0];
            p.pattern = e.pattern;
            p.variationB = e.variationB;
            p.repeatsLeft = std::max(1, e.repeats);
        }
        else
        {
            p.pattern = hearPattern;
            p.variationB = hearB;
        }
        return p;
    }

    const PlayPattern& Sequencer::patternAt(const Position& p) const noexcept
    {
        return bank.bank[static_cast<size_t>(std::clamp(p.pattern, 0, Project::patterns - 1))][p.variationB ? 1 : 0];
    }

    Sequencer::Position Sequencer::after(const Position& p, bool& tookQueue) const noexcept
    {
        tookQueue = false;
        Position n = p;
        if (p.ended)
            return n;
        n.step = p.step + 1;
        if (n.step < std::clamp(patternAt(p).length, 1, Pattern::steps))
            return n;
        // the pattern has played to its end
        n.step = 0;
        if (queued >= 0)
        {
            tookQueue = true;
            n.pattern = queued;
            n.variationB = queuedB;
            n.repeatsLeft = 1;
            return n;
        }
        if (bank.chainOn)
        {
            if (--n.repeatsLeft > 0)
                return n;
            ++n.chainIndex;
            if (n.chainIndex >= bank.chainLength)
            {
                if (! bank.loop)
                {
                    n.ended = true;
                    return n;
                }
                n.chainIndex = 0;
            }
            const auto& e = bank.chain[static_cast<size_t>(n.chainIndex)];
            n.pattern = e.pattern;
            n.variationB = e.variationB;
            n.repeatsLeft = std::max(1, e.repeats);
        }
        return n;
    }

    StepOut Sequencer::resolve(const Position& p, const StepOut& before, const Position& next) const noexcept
    {
        StepOut o;
        if (p.ended || p.step < 0)
            return o;
        const auto& pattern = patternAt(p);
        const Step& s = pattern.step[static_cast<size_t>(p.step)];
        const bool nextTies = ! next.ended && next.step >= 0 && patternAt(next).step[static_cast<size_t>(next.step)].gate == Gate::tie;
        switch (s.gate)
        {
            case Gate::rest:
                break;
            case Gate::tie:
                if (before.sounds)
                {
                    o.sounds = true;
                    o.continues = true;
                    o.volts = before.volts;
                    o.accent = before.accent;
                    o.slide = before.slide;
                }
                break;
            case Gate::note:
                o.sounds = true;
                o.volts = noteVolts(s.key() + transpose);
                o.accent = s.accent;
                o.continues = before.sounds && before.holds;
                o.slide = o.continues; // the slide switch on while the pitch glides from the note before
                break;
        }
        o.holds = o.sounds && (s.slide || nextTies);
        for (size_t k = 0; k < static_cast<size_t>(knobCount); ++k)
        {
            const float lock = s.lock[k];
            const float moved = pattern.motion[k][static_cast<size_t>(p.step)];
            o.knob[k] = lock >= 0.0f ? lock : moved;
        }
        return o;
    }

    void Sequencer::stepTo(std::int64_t sixteenth) noexcept
    {
        if (! startedAny || sixteenth != lastSixteenth + 1)
        {
            pos = first();
            now = {};
        }
        bool took = false;
        const Position next = after(pos, took);
        if (took)
        {
            hearPattern = queued;
            hearB = queuedB;
            queued = -1;
        }
        pos = next;
        bool ignored = false;
        const Position peek = after(pos, ignored);
        now = resolve(pos, now, peek);
        lastSixteenth = sixteenth;
        startedAny = true;
    }

    bool Sequencer::gateAt(double fraction, double secondsPerStep) const noexcept
    {
        if (! now.sounds)
            return false;
        const double lag = secondsPerStep > 0.0 ? gateLagSeconds / secondsPerStep : 0.0;
        const bool risen = now.continues || fraction >= lag;
        const bool fallen = ! now.holds && fraction >= 0.5 + lag; // on for three of the step's six clocks
        return risen && ! fallen;
    }
}
