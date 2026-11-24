import json, re, sys
log, out = sys.argv[1], sys.argv[2]
pieces = {}
for line in open(log, encoding="utf-8", errors="replace"):
    m = re.search(r'"EXPORT (\d+) (.*)", source:', line)
    if m:
        pieces[int(m.group(1))] = m.group(2)
text = "".join(pieces[i] for i in sorted(pieces))
# the log escapes quotes and backslashes as a C string
text = text.encode("utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8")
data = json.loads(text)
json.dump(data, open(out, "w", encoding="utf-8"), separators=(",", ":"))
print(len(pieces), "pieces;", {k: (len(v) if isinstance(v, list) else v) for k, v in data.items() if k != "colours" and k != "sheet"}, data["sheet"])
