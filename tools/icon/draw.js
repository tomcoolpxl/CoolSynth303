
const ORANGE = "#f0782a";
let uid = 0;

function plateSilver(id) {
  return `<defs><linearGradient id="pl${id}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#eceef0"/><stop offset="1" stop-color="#c5c9cd"/></linearGradient>
    <pattern id="br${id}" width="120" height="2" patternUnits="userSpaceOnUse"><rect width="120" height="1" fill="#ffffff" opacity="0.10"/></pattern></defs>
    <rect width="120" height="120" rx="26" fill="url(#pl${id})"/><rect width="120" height="120" rx="26" fill="url(#br${id})"/>
    <rect x="0.75" y="0.75" width="118.5" height="118.5" rx="25.5" fill="none" stroke="#868a90" stroke-width="1.5"/>`;
}
function plateCharcoal(id) {
  return `<defs><linearGradient id="pc${id}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2b2a28"/><stop offset="1" stop-color="#171615"/></linearGradient></defs>
    <rect width="120" height="120" rx="26" fill="url(#pc${id})"/>
    <rect x="0.75" y="0.75" width="118.5" height="118.5" rx="25.5" fill="none" stroke="#000" stroke-opacity="0.6" stroke-width="1.5"/>`;
}
function ticks(cx, cy, r, color, w, count = 11) {
  let s = "";
  for (let i = 0; i < count; i++) {
    const a = (-150 + 300 * i / (count - 1)) * Math.PI / 180;
    const r1 = r, r2 = r + (i % 5 === 0 ? 7 : 4.5);
    s += `<line x1="${cx + r1 * Math.sin(a)}" y1="${cy - r1 * Math.cos(a)}" x2="${cx + r2 * Math.sin(a)}" y2="${cy - r2 * Math.cos(a)}" stroke="${color}" stroke-width="${w}" stroke-linecap="round"/>`;
  }
  return s;
}
function arcPath(cx, cy, r, a0, a1) {
  const p = (a) => [cx + r * Math.sin(a * Math.PI / 180), cy - r * Math.cos(a * Math.PI / 180)];
  const [x0, y0] = p(a0), [x1, y1] = p(a1);
  return `M${x0} ${y0} A${r} ${r} 0 ${a1 - a0 > 180 ? 1 : 0} 1 ${x1} ${y1}`;
}
/** Our 303 knob: a black knurled skirt, a turned aluminium cap, an orange pointer. */
function knob303(cx, cy, r, value) {
  const id = ++uid;
  const circ = 2 * Math.PI * r * 0.86;
  const a = (-150 + 300 * value) * Math.PI / 180;
  return `<defs>
      <radialGradient id="sh${id}" cx="0.5" cy="0.3" r="0.6"><stop offset="0" stop-color="#fff" stop-opacity="0.16"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></radialGradient>
      <linearGradient id="al${id}" x1="0" y1="0" x2="1" y2="1">
        <stop offset="0" stop-color="#8e9196"/><stop offset="0.12" stop-color="#eef0f2"/><stop offset="0.25" stop-color="#a3a6ab"/>
        <stop offset="0.38" stop-color="#e4e6e9"/><stop offset="0.5" stop-color="#8e9196"/><stop offset="0.62" stop-color="#f1f2f4"/>
        <stop offset="0.75" stop-color="#9fa2a7"/><stop offset="0.88" stop-color="#e1e3e6"/><stop offset="1" stop-color="#8e9196"/></linearGradient></defs>
    <circle cx="${cx}" cy="${cy + r * 0.08}" r="${r * 1.03}" fill="#000" opacity="0.45"/>
    <circle cx="${cx}" cy="${cy}" r="${r}" fill="#0d0d0e"/>
    <circle cx="${cx}" cy="${cy}" r="${r * 0.86}" fill="none" stroke="#2a2b2e" stroke-width="${r * 0.26}" stroke-dasharray="${circ / 90} ${circ / 90}"/>
    <circle cx="${cx}" cy="${cy}" r="${r}" fill="url(#sh${id})"/>
    <circle cx="${cx}" cy="${cy}" r="${r * 0.6}" fill="url(#al${id})" stroke="#000" stroke-opacity="0.6" stroke-width="${r * 0.04}"/>
    <line x1="${cx + r * 0.66 * Math.sin(a)}" y1="${cy - r * 0.66 * Math.cos(a)}" x2="${cx + r * 0.97 * Math.sin(a)}" y2="${cy - r * 0.97 * Math.cos(a)}" stroke="${ORANGE}" stroke-width="${r * 0.13}" stroke-linecap="round"/>`;
}
/** The 808's knob: a warm dark cap lit from the upper left, a cream pointer; or the orange accent cap with a dark one. */
function knob808(cx, cy, r, value, accent) {
  const id = ++uid;
  const a = (-150 + 300 * value) * Math.PI / 180;
  const [c0, c1, ptr] = accent ? ["#f7a05a", "#c4521c", "#1a1714"] : ["#5a554e", "#1d1b18", "#ece4d2"];
  return `<defs><radialGradient id="k${id}" cx="0.4" cy="0.3" r="0.8"><stop offset="0" stop-color="${c0}"/><stop offset="1" stop-color="${c1}"/></radialGradient></defs>
    <circle cx="${cx}" cy="${cy + r * 0.08}" r="${r * 1.03}" fill="#000" opacity="0.55"/>
    <circle cx="${cx}" cy="${cy}" r="${r}" fill="url(#k${id})" stroke="#0e0d0c" stroke-width="${r * 0.05}"/>
    <circle cx="${cx}" cy="${cy}" r="${r * 0.62}" fill="none" stroke="#fff" stroke-opacity="0.08" stroke-width="${r * 0.04}"/>
    <line x1="${cx + r * 0.2 * Math.sin(a)}" y1="${cy - r * 0.2 * Math.cos(a)}" x2="${cx + r * 0.86 * Math.sin(a)}" y2="${cy - r * 0.86 * Math.cos(a)}" stroke="${ptr}" stroke-width="${r * 0.14}" stroke-linecap="round"/>`;
}
const num = (t, x, y, size, fill) => `<text x="${x}" y="${y}" text-anchor="middle" font-family="Archivo" font-weight="800" font-size="${size}" fill="${fill}" letter-spacing="-0.5">${t}</text>`;

const d303 = [
  ["A. The earlier one, in the 303's own look", "The same layout as before, now on the silver plate: our 303 knob (knurled black skirt, turned aluminium cap, orange pointer), dark printed ticks, \"303\" in the panel's black.",
    (s) => { const id = ++uid; return plateSilver(id) + (s ? ticks(60, 60, 41, "#2b2d31", 3.2) + knob303(60, 60, 34, 0.68)
      : ticks(60, 45, 30, "#2b2d31", 2.4) + knob303(60, 45, 24, 0.68) + num("303", 60, 104, 30, "#1c1d20")); }],
  ["B. The knob with its orange arc", "As the panel draws it: the value as a lit orange arc round the knob, bigger knob, \"303\" smaller in black.",
    (s) => { const id = ++uid; const big = s ? [60, 60, 32, 42] : [60, 47, 27, 35];
      return plateSilver(id) + `<path d="${arcPath(big[0], big[1], big[3], -150, 150)}" fill="none" stroke="#2b2d31" stroke-width="${s ? 6 : 4.5}" stroke-linecap="round"/>
        <path d="${arcPath(big[0], big[1], big[3], -150, 55)}" fill="none" stroke="${ORANGE}" stroke-width="${s ? 6 : 4.5}" stroke-linecap="round"/>` +
        knob303(big[0], big[1], big[2], 0.68) + (s ? "" : num("303", 60, 106, 22, "#1c1d20")); }],
  ["C. Two knobs, as CoolSynth's", "The family's two knobs (cutoff and resonance) side by side, \"303\" below, on silver.",
    (s) => { const id = ++uid; return plateSilver(id) + (s ? knob303(37, 60, 19, 0.3) + knob303(83, 60, 19, 0.75)
      : ticks(36, 44, 21, "#2b2d31", 2) + knob303(36, 44, 16, 0.3) + ticks(84, 44, 21, "#2b2d31", 2) + knob303(84, 44, 16, 0.75) + num("303", 60, 104, 30, "#1c1d20")); }],
  ["D. From scratch: the squelch", "No knob: the sound itself. A dark display set in the silver, an orange saw whose every ramp rings at the filter's resonance (the acid squelch), \"303\" under it.",
    (s) => { const id = ++uid; let d = ""; const y0 = s ? 62 : 50, h = s ? 30 : 22, x0 = 18, w = 84;
      const pts = []; const cycles = 2.5; for (let i = 0; i <= 260; i++) { const t = i / 260, ph = (t * cycles) % 1;
        const saw = 1 - 2 * ph, ring = Math.exp(-ph * 5) * Math.sin(ph * 2 * Math.PI * 4.5) * 0.9; pts.push([x0 + t * w, y0 - (saw * 0.45 + ring * 0.55) * h]); }
      d = "M" + pts.map(p => p[0].toFixed(2) + " " + p[1].toFixed(2)).join(" L");
      const scr = s ? [12, 22, 96, 76] : [12, 14, 96, 64];
      return plateSilver(id) + `<rect x="${scr[0]}" y="${scr[1]}" width="${scr[2]}" height="${scr[3]}" rx="10" fill="#2e3135" stroke="#1d1f22" stroke-width="1.5"/>
        <path d="${d}" fill="none" stroke="${ORANGE}" stroke-width="${s ? 5 : 3.6}" stroke-linejoin="round" stroke-linecap="round"/>` + (s ? "" : num("303", 60, 106, 24, "#1c1d20")); }],
];
const d808 = [
  ["A. The earlier layout, in the 808's look", "The 808's own knob (a warm dark cap, cream pointer) with cream printed ticks on the charcoal panel, \"808\" in its cream.",
    (s) => { const id = ++uid; return plateCharcoal(id) + (s ? ticks(60, 60, 41, "#cfc7b3", 3.2) + knob808(60, 60, 34, 0.68, false)
      : ticks(60, 45, 30, "#cfc7b3", 2.4) + knob808(60, 45, 24, 0.68, false) + num("808", 60, 104, 30, "#f1ebde")); }],
  ["B. The orange accent knob", "The 808's orange ACCENT knob, the one that stands out on its panel, with its dark pointer; \"808\" in cream.",
    (s) => { const id = ++uid; return plateCharcoal(id) + (s ? ticks(60, 60, 41, "#cfc7b3", 3.2) + knob808(60, 60, 34, 0.68, true)
      : ticks(60, 45, 30, "#cfc7b3", 2.4) + knob808(60, 45, 24, 0.68, true) + num("808", 60, 104, 30, "#f1ebde")); }],
  ["C. Two knobs, as CoolSynth's", "The family's two knobs: the orange accent beside a dark one, \"808\" below.",
    (s) => { const id = ++uid; return plateCharcoal(id) + (s ? knob808(37, 60, 19, 0.3, true) + knob808(83, 60, 19, 0.75, false)
      : ticks(36, 44, 21, "#cfc7b3", 2) + knob808(36, 44, 16, 0.3, true) + ticks(84, 44, 21, "#cfc7b3", 2) + knob808(84, 44, 16, 0.75, false) + num("808", 60, 104, 30, "#f1ebde")); }],
  ["D. From scratch: the kick", "No knob: the bass drum's long falling sine, in orange on a dark display set in the charcoal, \"808\" under it.",
    (s) => { const id = ++uid; const y0 = s ? 60 : 47, h = s ? 34 : 26, x0 = 16, w = 88; const pts = [];
      for (let i = 0; i <= 300; i++) { const t = i / 300; const f = 2.2 + 4.5 * Math.exp(-t * 6); const ph = 2 * Math.PI * (2.2 * t + 0.75 * (1 - Math.exp(-t * 6)));
        pts.push([x0 + t * w, y0 - Math.sin(ph) * Math.exp(-t * 2.2) * h]); }
      const d = "M" + pts.map(p => p[0].toFixed(2) + " " + p[1].toFixed(2)).join(" L");
      const scr = s ? [10, 20, 100, 80] : [10, 12, 100, 70];
      return plateCharcoal(id) + `<rect x="${scr[0]}" y="${scr[1]}" width="${scr[2]}" height="${scr[3]}" rx="10" fill="#0f0f0e" stroke="#000" stroke-width="1.5"/>
        <path d="${d}" fill="none" stroke="${ORANGE}" stroke-width="${s ? 5 : 3.6}" stroke-linejoin="round" stroke-linecap="round"/>` + (s ? "" : num("808", 60, 106, 24, "#f1ebde")); }],
];

// the user's picks (303 A, 808 B), with room between the knob and the number: the knob higher and a little smaller
const pick303 = [["303: A, with room under the knob", "Our 303 knob over \"303\" on the silver plate; the knob moved up and made a little smaller so the number has room.",
  (s) => { const id = ++uid; return plateSilver(id) + (s ? ticks(60, 60, 41, "#2b2d31", 3.2) + knob303(60, 60, 34, 0.68)
    : ticks(60, 39, 26, "#2b2d31", 2.2) + knob303(60, 39, 20.5, 0.68) + num("303", 60, 104, 28, "#1c1d20")); }]];
const pick808 = [["808: B, with room under the knob", "The 808's orange accent knob over \"808\" on the charcoal plate; the knob moved up and made a little smaller so the number has room.",
  (s) => { const id = ++uid; return plateCharcoal(id) + (s ? ticks(60, 60, 41, "#cfc7b3", 3.2) + knob808(60, 60, 34, 0.68, true)
    : ticks(60, 39, 26, "#cfc7b3", 2.2) + knob808(60, 39, 20.5, 0.68, true) + num("808", 60, 104, 28, "#f1ebde")); }]];
