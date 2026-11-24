#include "Pattern.h"

#include <algorithm>
#include <cmath>

namespace cs303
{
    namespace
    {
        constexpr const char* knobNames[] = { "Cutoff", "Resonance", "Env mod", "Decay", "Accent" };
        constexpr const char* knobKeys[] = { "cutoff", "resonance", "envMod", "decay", "accent" };

        /** A small deterministic source (xorshift32): the same seed gives the same line on every machine. */
        struct Random
        {
            std::uint32_t s;
            explicit Random(std::uint32_t seed) : s(seed == 0 ? 0x303u : seed) {}
            std::uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
            double unit() noexcept { return static_cast<double>(next() >> 8) / 16777216.0; } // [0, 1)
            int below(int n) noexcept { return static_cast<int>(unit() * n); }
        };

        float clamp01OrNone(const juce::var& v)
        {
            if (v.isVoid() || v.isUndefined())
                return none;
            const auto f = static_cast<float>(static_cast<double>(v));
            return f < 0.0f ? none : std::min(1.0f, f);
        }
        float clamp01(const juce::var& v, float fallback)
        {
            if (v.isVoid() || v.isUndefined())
                return fallback;
            return juce::jlimit(0.0f, 1.0f, static_cast<float>(static_cast<double>(v)));
        }
        int clampInt(const juce::var& v, int lo, int hi, int fallback)
        {
            if (v.isVoid() || v.isUndefined())
                return fallback;
            return juce::jlimit(lo, hi, static_cast<int>(v));
        }
        bool boolOr(const juce::var& v, bool fallback) { return v.isVoid() || v.isUndefined() ? fallback : static_cast<bool>(v); }
    }

    const char* knobName(Knob k) noexcept { return knobNames[static_cast<int>(k)]; }
    const char* knobKey(Knob k) noexcept { return knobKeys[static_cast<int>(k)]; }
    bool lockable(Knob k) noexcept { return k != Knob::resonance; }

    void Step::setKey(int k) noexcept
    {
        k = juce::jlimit(0, 36, k);
        octave = k < 12 ? -1 : k < 24 ? 0 : 1;
        note = k - (octave + 1) * 12;
    }

    bool Step::hasLocks() const noexcept
    {
        return std::any_of(lock.begin(), lock.end(), [](float v) { return v >= 0.0f; });
    }

    Pattern::Pattern()
    {
        for (auto& lane : motion)
            lane.fill(none);
    }

    bool Pattern::hasMotion(Knob k) const noexcept
    {
        const auto& lane = motion[static_cast<size_t>(k)];
        return std::any_of(lane.begin(), lane.end(), [](float v) { return v >= 0.0f; });
    }

    double delayBeats(int division) noexcept
    {
        constexpr double beats[] = { 0.25, 0.5, 0.75, 1.0 };
        return beats[juce::jlimit(0, 3, division)];
    }
    const char* delayDivisionName(int division) noexcept
    {
        constexpr const char* names[] = { "1/16", "1/8", "3/16", "1/4" };
        return names[juce::jlimit(0, 3, division)];
    }

    const char* scaleName(Scale s) noexcept
    {
        constexpr const char* names[] = { "minor", "phrygian", "dorian", "chromatic" };
        return names[juce::jlimit(0, 3, static_cast<int>(s))];
    }
    const std::vector<int>& scaleDegrees(Scale s) noexcept
    {
        static const std::vector<int> degrees[] = {
            { 0, 2, 3, 5, 7, 8, 10 },
            { 0, 1, 3, 5, 7, 8, 10 },
            { 0, 2, 3, 5, 7, 9, 10 },
            { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
        };
        return degrees[juce::jlimit(0, 3, static_cast<int>(s))];
    }

    // ---------------------------------------------------------------- JSON
    juce::var toVar(const Pattern& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty("name", juce::String(p.name));
        o->setProperty("length", p.length);
        juce::Array<juce::var> steps;
        for (const auto& s : p.step)
        {
            auto* so = new juce::DynamicObject();
            so->setProperty("note", s.note);
            so->setProperty("octave", s.octave);
            so->setProperty("accent", s.accent);
            so->setProperty("slide", s.slide);
            so->setProperty("gate", s.gate == Gate::note ? "note" : s.gate == Gate::tie ? "tie" : "rest");
            if (s.hasLocks())
            {
                auto* lo = new juce::DynamicObject();
                for (int k = 0; k < knobCount; ++k)
                    if (s.lock[static_cast<size_t>(k)] >= 0.0f)
                        lo->setProperty(knobKeys[k], s.lock[static_cast<size_t>(k)]);
                so->setProperty("locks", juce::var(lo));
            }
            steps.add(juce::var(so));
        }
        o->setProperty("steps", steps);
        auto* mo = new juce::DynamicObject();
        for (int k = 0; k < knobCount; ++k)
            if (p.hasMotion(static_cast<Knob>(k)))
            {
                juce::Array<juce::var> lane;
                for (float v : p.motion[static_cast<size_t>(k)])
                    lane.add(v >= 0.0f ? juce::var(v) : juce::var());
                mo->setProperty(knobKeys[k], lane);
            }
        o->setProperty("motion", juce::var(mo));
        return juce::var(o);
    }

    Pattern patternFromVar(const juce::var& v)
    {
        Pattern p;
        if (!v.isObject())
            return p;
        p.name = v["name"].toString().toStdString();
        p.length = clampInt(v["length"], 1, Pattern::steps, Pattern::steps);
        if (const auto* steps = v["steps"].getArray())
            for (int i = 0; i < std::min(Pattern::steps, steps->size()); ++i)
            {
                const auto& sv = steps->getReference(i);
                auto& s = p.step[static_cast<size_t>(i)];
                s.note = clampInt(sv["note"], 0, 12, 0);
                s.octave = clampInt(sv["octave"], -1, 1, 0);
                s.accent = boolOr(sv["accent"], false);
                s.slide = boolOr(sv["slide"], false);
                const auto g = sv["gate"].toString();
                s.gate = g == "tie" ? Gate::tie : g == "rest" ? Gate::rest : Gate::note;
                const auto locks = sv["locks"];
                for (int k = 0; k < knobCount; ++k)
                    s.lock[static_cast<size_t>(k)] = lockable(static_cast<Knob>(k)) ? clamp01OrNone(locks[knobKeys[k]]) : none;
            }
        const auto motion = v["motion"];
        for (int k = 0; k < knobCount; ++k)
            if (const auto* lane = motion[knobKeys[k]].getArray())
                for (int i = 0; i < std::min(Pattern::steps, lane->size()); ++i)
                    p.motion[static_cast<size_t>(k)][static_cast<size_t>(i)] = clamp01OrNone(lane->getReference(i));
        return p;
    }

    juce::var toVar(const Project& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty("format", "CoolSynth303 project");
        o->setProperty("version", 1);
        o->setProperty("name", juce::String(p.name));
        o->setProperty("bpm", p.bpm);
        o->setProperty("root", p.root);
        o->setProperty("scale", scaleName(p.scale));
        juce::Array<juce::var> bank;
        for (const auto& pair : p.bank)
        {
            auto* po = new juce::DynamicObject();
            po->setProperty("a", toVar(pair[0]));
            po->setProperty("b", toVar(pair[1]));
            bank.add(juce::var(po));
        }
        o->setProperty("patterns", bank);
        juce::Array<juce::var> chain;
        for (const auto& e : p.chain)
        {
            auto* eo = new juce::DynamicObject();
            eo->setProperty("pattern", e.pattern);
            eo->setProperty("variation", e.variationB ? "B" : "A");
            eo->setProperty("repeats", e.repeats);
            chain.add(juce::var(eo));
        }
        o->setProperty("chain", chain);
        o->setProperty("chainOn", p.chainOn);
        o->setProperty("loop", p.loop);
        auto* k = new juce::DynamicObject();
        const auto& pn = p.panel;
        k->setProperty("tuning", pn.tuning);
        k->setProperty("cutoff", pn.cutoff);
        k->setProperty("resonance", pn.resonance);
        k->setProperty("envMod", pn.envMod);
        k->setProperty("decay", pn.decay);
        k->setProperty("accent", pn.accent);
        k->setProperty("volume", pn.volume);
        k->setProperty("square", pn.square);
        k->setProperty("driveOn", pn.driveOn);
        k->setProperty("drive", pn.drive);
        k->setProperty("tone", pn.tone);
        k->setProperty("delayOn", pn.delayOn);
        k->setProperty("delayDivision", pn.delayDivision);
        k->setProperty("feedback", pn.feedback);
        k->setProperty("mix", pn.mix);
        o->setProperty("panel", juce::var(k));
        return juce::var(o);
    }

    Project projectFromVar(const juce::var& v)
    {
        Project p;
        if (!v.isObject())
            return p;
        p.name = v["name"].isString() ? v["name"].toString().toStdString() : p.name;
        p.bpm = v["bpm"].isVoid() ? p.bpm : juce::jlimit(40.0, 300.0, static_cast<double>(v["bpm"]));
        p.root = clampInt(v["root"], 0, 11, 0);
        const auto sc = v["scale"].toString();
        for (int s = 0; s < static_cast<int>(Scale::count); ++s)
            if (sc == scaleName(static_cast<Scale>(s)))
                p.scale = static_cast<Scale>(s);
        if (const auto* bank = v["patterns"].getArray())
            for (int i = 0; i < std::min(Project::patterns, bank->size()); ++i)
            {
                p.bank[static_cast<size_t>(i)][0] = patternFromVar(bank->getReference(i)["a"]);
                p.bank[static_cast<size_t>(i)][1] = patternFromVar(bank->getReference(i)["b"]);
            }
        if (const auto* chain = v["chain"].getArray())
        {
            p.chain.clear();
            for (const auto& e : *chain)
                if (p.chain.size() < 64)
                    p.chain.push_back({ clampInt(e["pattern"], 0, Project::patterns - 1, 0), e["variation"].toString() == "B",
                                        clampInt(e["repeats"], 1, 16, 1) });
            if (p.chain.empty())
                p.chain.push_back({});
        }
        p.chainOn = boolOr(v["chainOn"], false);
        p.loop = boolOr(v["loop"], true);
        const auto k = v["panel"];
        auto& pn = p.panel;
        const Panel d;
        pn.tuning = clamp01(k["tuning"], d.tuning);
        pn.cutoff = clamp01(k["cutoff"], d.cutoff);
        pn.resonance = clamp01(k["resonance"], d.resonance);
        pn.envMod = clamp01(k["envMod"], d.envMod);
        pn.decay = clamp01(k["decay"], d.decay);
        pn.accent = clamp01(k["accent"], d.accent);
        pn.volume = clamp01(k["volume"], d.volume);
        pn.square = boolOr(k["square"], d.square);
        pn.driveOn = boolOr(k["driveOn"], d.driveOn);
        pn.drive = clamp01(k["drive"], d.drive);
        pn.tone = clamp01(k["tone"], d.tone);
        pn.delayOn = boolOr(k["delayOn"], d.delayOn);
        pn.delayDivision = clampInt(k["delayDivision"], 0, 3, d.delayDivision);
        pn.feedback = clamp01(k["feedback"], d.feedback);
        pn.mix = clamp01(k["mix"], d.mix);
        return p;
    }

    juce::String toJson(const Project& p) { return juce::JSON::toString(toVar(p)); }
    Project projectFromJson(const juce::String& json) { return projectFromVar(juce::JSON::parse(json)); }

    // ---------------------------------------------------------------- the tools
    void generate(Pattern& p, std::uint32_t seed, Scale scale, int root)
    {
        Random r(seed);
        const auto& sc = scaleDegrees(scale);
        for (int i = 0; i < Pattern::steps; ++i)
        {
            auto& s = p.step[static_cast<size_t>(i)];
            const double g = r.unit();
            s.gate = i == 0 ? Gate::note : g < 0.68 ? Gate::note : g < 0.84 ? Gate::rest : Gate::tie;
            const int degree = r.unit() < 0.35 ? 0 : sc[static_cast<size_t>(r.below(static_cast<int>(sc.size())))];
            const int n = (degree + root) % 12;
            const double o = r.unit();
            s.octave = o < 0.16 ? 1 : o < 0.26 ? -1 : 0;
            s.note = n;
            s.accent = s.gate == Gate::note && r.unit() < 0.27;
            s.slide = s.gate != Gate::rest && r.unit() < 0.2;
            s.lock.fill(none);
        }
    }

    void mutate(Pattern& p, std::uint32_t seed, Scale scale, int root, MutateWhat what)
    {
        Random r(seed);
        const auto& sc = scaleDegrees(scale);
        for (int k = 0; k < 4; ++k)
        {
            auto& s = p.step[static_cast<size_t>(r.below(Pattern::steps))];
            if (what.notes && s.gate != Gate::rest)
                s.note = (sc[static_cast<size_t>(r.below(static_cast<int>(sc.size())))] + root) % 12;
            if (what.accents)
                s.accent = r.unit() < 0.3;
            if (what.slides)
                s.slide = r.unit() < 0.25;
            if (what.rests)
            {
                const double g = r.unit();
                s.gate = g < 0.2 ? Gate::rest : g < 0.36 ? Gate::tie : Gate::note;
            }
        }
    }

    void snapToKey(Pattern& p, Scale scale, int root)
    {
        const auto& sc = scaleDegrees(scale);
        for (auto& s : p.step)
        {
            const int rel = ((s.note % 12) - root + 12) % 12;
            int best = sc.front(), bestDistance = 12;
            for (int d : sc)
                for (int candidate : { d, d + 12, d - 12 })
                    if (const int dist = std::abs(candidate - rel); dist < bestDistance)
                        bestDistance = dist, best = candidate;
            // keep the step's place: the nearest note may be across the C
            const int key = s.key() + (best - rel);
            s.setKey(key < 0 ? key + 12 : key > 36 ? key - 12 : key);
        }
    }

    void shift(Pattern& p, int direction)
    {
        const int n = p.length;
        if (n < 2)
            return;
        auto copy = p.step;
        auto motionCopy = p.motion;
        for (int i = 0; i < n; ++i)
        {
            const int from = ((i - direction) % n + n) % n;
            p.step[static_cast<size_t>(i)] = copy[static_cast<size_t>(from)];
            for (int k = 0; k < knobCount; ++k)
                p.motion[static_cast<size_t>(k)][static_cast<size_t>(i)] = motionCopy[static_cast<size_t>(k)][static_cast<size_t>(from)];
        }
    }

    void semitone(Pattern& p, int direction)
    {
        for (auto& s : p.step)
        {
            const int key = s.key() + direction;
            if (key >= 0 && key <= 36)
                s.setKey(key);
        }
    }

    Project defaultProject()
    {
        Project p;
        const char* names[] = { "Line 1", "Line 2", "Line 3", "Line 4", "Line 5" };
        const std::uint32_t seeds[] = { 11, 23, 37, 52, 71 };
        for (int i = 0; i < 5; ++i)
        {
            auto& a = p.pattern(i, false);
            generate(a, seeds[i], Scale::minor, 0);
            a.name = names[i];
            p.pattern(i, true) = a;
            mutate(p.pattern(i, true), seeds[i] * 7u, Scale::minor, 0, {});
            p.pattern(i, true).name = std::string(names[i]) + " B";
        }
        for (int i = 0; i < Pattern::steps; ++i)
            p.pattern(0, false).motion[static_cast<size_t>(Knob::cutoff)][static_cast<size_t>(i)] =
                static_cast<float>(0.25 + 0.5 * (0.5 - 0.5 * std::cos(i / 15.0 * juce::MathConstants<double>::pi)));
        p.chain = { { 0, false, 2 }, { 1, false, 1 }, { 0, false, 2 }, { 2, false, 1 } };
        return p;
    }
}
