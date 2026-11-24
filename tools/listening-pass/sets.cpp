// Plays the stock TB-303 voice through Din Sync's 25 sets: per knob position (25, 50, 75, 100 %) a plain saw, an
// accented saw, a plain square, an accented square, low C, each held 1.3 s, 3 s apart from 1.14 s, 49.125 s in all.
// Writes raw 32-bit float mono at 96 kHz per set: out\SET-xn.f32.
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "synth/v3/Tb303Voice.h"

int main(int argc, char** argv)
{
    using coolsynth::synth::v3::Tb303Voice;
    const double rate = 96000.0;
    const char* knobsNames[] = {"cutoff", "resonance", "envMod", "decay", "accent"};
    const double base[] = {0.0, 0.25, 0.5, 0.75, 1.0};
    // sets.exe [row [set p1 p2 p3 p4 tag]]: one row of sets (A to E), or all; or one set with the turned knob at four
    // positions of its own (where Din Sync's hand-set positions are in doubt), written as SET-xn-tag.f32.
    const int only = argc > 1 ? argv[1][0] - 'A' : -1;
    const int onlySet = argc > 2 ? std::atoi(argv[2]) - 1 : -1;
    double positions[] = {0.25, 0.5, 0.75, 1.0};
    std::string tag;
    if (argc > 7)
    {
        for (int p = 0; p < 4; ++p)
            positions[p] = std::atof(argv[3 + p]);
        tag = std::string("-") + argv[7];
    }
    for (int s = 0; s < 5; ++s)
        if (only < 0 || s == only)
        for (int turned = 0; turned < 5; ++turned)
        if (onlySet < 0 || turned == onlySet)
        {
            const std::string name = std::string("SET-") + char('A' + s) + char('1' + turned) + tag;
            auto v = std::make_unique<Tb303Voice>();
            v->prepare(rate);
            Tb303Voice::Knobs k;
            k.cutoff = k.resonance = k.envMod = k.decay = k.accent = base[s];
            auto setTurn = [&](double turn, bool square) {
                double* knob[] = {&k.cutoff, &k.resonance, &k.envMod, &k.decay, &k.accent};
                *knob[turned] = turn;
                k.square = square;
                v->setKnobs(k);
            };
            setTurn(positions[0], false);
            v->settle(2.0);
            const int n = int(49.125 * rate);
            std::vector<float> out(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i)
            {
                const double t = i / rate;
                const int note = int((t - 1.14 + 1.0) / 3.0); // the next note's settings from 1 s before it
                if (i == 0 || (int((t - 1.14 + 1.0) / 3.0) != int((t - 1.0 / rate - 1.14 + 1.0) / 3.0)))
                    if (note >= 0 && note < 16)
                        setTurn(positions[note / 4], (note % 4) >= 2);
                const int playing = int((t - 1.14) / 3.0);
                const double since = t - 1.14 - 3.0 * playing;
                const bool gate = t >= 1.14 && playing < 16 && since < 1.3;
                const bool accent = gate && (playing % 2) == 1;
                out[size_t(i)] = float(v->process(2.0, gate, accent, false));
            }
            const std::string path = "out\\" + name + ".f32";
            if (FILE* f = std::fopen(path.c_str(), "wb"))
            {
                std::fwrite(out.data(), sizeof(float), out.size(), f);
                std::fclose(f);
            }
            std::printf("%s (%s turned, the others at %.0f %%) done\n", name.c_str(), knobsNames[turned], 100 * base[s]);
            std::fflush(stdout);
        }
}
