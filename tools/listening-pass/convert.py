"""
Turns the set renders (raw 32-bit float mono at 96 kHz, out/SET-xn.f32 from sets.cpp) into WAVs to A/B against Din
Sync's recordings of a real TB-303, at one level for every set so the sets' own level differences stay audible.

    python -I tools/listening-pass/convert.py <renders folder> <Din Sync folder> <out folder>

The level: the median over all 400 notes of the real note's level against ours (30 to 80 ms after its onset, as
compare_sets.py measures). A 5 Hz high-pass takes out the voice's DC, as the real unit's output capacitor does.
Standard library only.
"""
import array
import math
import os
import statistics
import struct
import sys
import wave

RATE = 96000


def real_levels(path):
    w = wave.open(path)
    rate, n = w.getframerate(), w.getnframes()
    left = struct.unpack("<%dh" % (n * 2), w.readframes(n))[0::2]
    out = []
    for note in range(16):
        guess = 1.14 + 3.0 * note
        lo = int((guess - 0.1) * rate)
        seg = left[lo:lo + int(0.5 * rate)]
        peak = max(abs(v) for v in seg)
        onset = lo + next(i for i, v in enumerate(seg) if abs(v) > 0.1 * peak)
        out.append(rms(left[onset + int(0.03 * rate):onset + int(0.08 * rate)]) / 32768.0)
    return out


def rms(x):
    m = sum(x) / len(x)
    return math.sqrt(sum((v - m) ** 2 for v in x) / len(x)) + 1e-12


def main():
    renders, dinsync, out = sys.argv[1:4]
    os.makedirs(out, exist_ok=True)
    names = sorted(f[:-4] for f in os.listdir(renders) if f.endswith(".f32"))
    ours, ratios = {}, []
    for name in names:
        x = array.array("f")
        with open(os.path.join(renders, name + ".f32"), "rb") as f:
            x.frombytes(f.read())
        # The 5 Hz high-pass, one pole.
        a = math.exp(-2 * math.pi * 5.0 / RATE)
        y, prev_in, prev_out = [0.0] * len(x), x[0], 0.0
        for i, v in enumerate(x):
            prev_out = a * (prev_out + v - prev_in)
            prev_in = v
            y[i] = prev_out
        ours[name] = y
        real = real_levels(os.path.join(dinsync, name + ".wav"))
        for note in range(16):
            at = int((1.14 + 3.0 * note) * RATE)
            ratios.append(real[note] / rms(y[at + int(0.03 * RATE):at + int(0.08 * RATE)]))
    gain = statistics.median(ratios)
    spread = [20 * math.log10(r / gain) for r in ratios]
    print("gain %.4g; the notes' level against the real unit's: 10 %% within %.1f dB, 90 %% within %.1f dB" % (
        gain, sorted(abs(s) for s in spread)[len(spread) // 10], sorted(abs(s) for s in spread)[9 * len(spread) // 10]))
    peak = max(max(abs(v) for v in y) for y in ours.values()) * gain
    print("loudest sample %.2f of full scale" % peak)
    for name, y in ours.items():
        pcm = array.array("h", (max(-32767, min(32767, int(round(v * gain * 32767)))) for v in y))
        with wave.open(os.path.join(out, name + "-coolsynth.wav"), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(pcm.tobytes())
    print("%d sets written to %s" % (len(ours), out))


if __name__ == "__main__":
    main()
