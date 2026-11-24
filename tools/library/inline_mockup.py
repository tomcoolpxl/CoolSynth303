"""Puts the library's data into CoolSynth's library mockup: reads docs/design/mockups/coolsynth-303-library.src.html
in the CoolSynth repository beside this one and writes coolsynth-303-library.html with library303.json inlined.

    python -I tools/library/inline_mockup.py [path to the CoolSynth repository]
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
coolsynth = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "..", "CoolSynth")
mockups = os.path.join(coolsynth, "docs", "design", "mockups")
with open(os.path.join(mockups, "coolsynth-303-library.src.html"), encoding="utf-8") as f:
    page = f.read()
with open(os.path.join(HERE, "..", "..", "resources", "library", "library303.json"), encoding="utf-8") as f:
    data = f.read().replace("</", "<\\/")
with open(os.path.join(mockups, "coolsynth-303-library.html"), "w", encoding="utf-8") as f:
    f.write(page.replace("/*LIBRARY*/", data))
print("written", os.path.join(mockups, "coolsynth-303-library.html"))
