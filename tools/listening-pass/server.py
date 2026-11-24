"""
The CoolSynth 303 listening pass: Din Sync's 25 recordings of a real TB-303 beside the stock voice playing the same
notes at the same knobs, switched while both play, and the user's marks kept in a JSON file Claude reads back.

    python -I tools/listening-pass/server.py <Din Sync folder> <renders folder> [--port 47630] [--marks marks.json]

Din Sync's chart: SET-A1 to A5 turn CUTOFF, RESONANCE, ENV MOD, DECAY, ACCENT from 25 to 100 %, the others at 0 %;
rows B to E the same with the others at 25, 50, 75 and 100 %. Each set: per position a plain saw, an accented saw, a
plain square, an accented square, low C, 3 s apart from 1.14 s.

    GET  /                     the page
    GET  /sets                 the sets and what each turns
    GET  /audio/SET-C3-real.wav, /audio/SET-C3-coolsynth.wav
    GET  /marks, POST /marks {"id", "v", "note"}
Serves 127.0.0.1 only. Standard library only.
"""
import argparse
import json
import os
import re
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
KNOBS = ["Cut Off", "Resonance", "Env Mod", "Decay", "Accent"]
ARGS = None


def sets():
    out = []
    for row, base in zip("ABCDE", (0, 25, 50, 75, 100)):
        for k, knob in enumerate(KNOBS):
            out.append({"id": "SET-%s%d" % (row, k + 1), "row": row, "base": base, "knob": knob})
    return out


def read_marks():
    try:
        with open(ARGS.marks, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def reply(self, code, payload, kind="application/json"):
        data = payload if isinstance(payload, bytes) else json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", kind)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            with open(os.path.join(HERE, "page.html"), "rb") as f:
                return self.reply(200, f.read(), "text/html; charset=utf-8")
        if self.path == "/sets":
            return self.reply(200, sets())
        if self.path == "/marks":
            return self.reply(200, read_marks())
        m = re.fullmatch(r"/audio/(SET-[A-E][1-5])-(real|coolsynth)\.wav", self.path)
        if m:
            name, side = m.groups()
            path = os.path.join(ARGS.dinsync, name + ".wav") if side == "real" else \
                os.path.join(ARGS.renders, name + "-coolsynth.wav")
            with open(path, "rb") as f:
                return self.reply(200, f.read(), "audio/wav")
        self.reply(404, {"error": "not found"})

    def do_POST(self):
        if self.path != "/marks":
            return self.reply(404, {"error": "not found"})
        length = int(self.headers.get("Content-Length", "0") or 0)
        request = json.loads(self.rfile.read(length) or b"{}")
        marks = read_marks()
        marks[request["id"]] = {"v": request.get("v", ""), "note": request.get("note", "")}
        with open(ARGS.marks, "w", encoding="utf-8") as f:
            json.dump(marks, f, indent=2, ensure_ascii=False)
        self.reply(200, {"ok": True})


def main():
    global ARGS
    parser = argparse.ArgumentParser()
    parser.add_argument("dinsync")
    parser.add_argument("renders")
    parser.add_argument("--port", type=int, default=47630)
    parser.add_argument("--marks", default=os.path.join(HERE, "marks.json"))
    ARGS = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", ARGS.port), Handler)
    print("Listening pass on http://127.0.0.1:%d" % ARGS.port, flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
