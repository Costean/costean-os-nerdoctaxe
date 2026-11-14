// Browser-side long memory. 1-minute buckets persisted in localStorage so charts survive reloads (24 h).
// Plus an "efficiency map": average GH/s and W observed at each frequency/voltage point. Nothing is sent anywhere.
const KEY = 'nx4.trend', LAB = 'nx4.lab', MAX = 1440;
const ls = typeof localStorage !== 'undefined' ? localStorage : { getItem: () => null, setItem: () => {} };
const load = (k, d) => { try { return JSON.parse(ls.getItem(k)) ?? d; } catch { return d; } };
const avg = (a, k) => { const v = a.map(p => p[k]).filter(x => x != null && !isNaN(x)); return v.length ? v.reduce((s, x) => s + x, 0) / v.length : null; };
const avgArr = (a, k) => { const n = Math.max(0, ...a.map(p => (p[k] || []).length)); return Array.from({ length: n }, (_, i) => { const v = a.map(p => p[k]?.[i]).filter(x => x != null); return v.length ? +(v.reduce((s, x) => s + x, 0) / v.length).toFixed(1) : null; }); };
export function bucket(pts) {
  const last = pts[pts.length - 1];
  return { t: Math.floor(last.t / 60000) * 60000, gh: avg(pts, 'gh'), w: avg(pts, 'w'), eff: avg(pts, 'eff'), asic: avg(pts, 'asic'), vr: avg(pts, 'vr'), fan: avg(pts, 'fan'),
    chr: avgArr(pts, 'chr'), ct: avgArr(pts, 'ct'), acc: last.acc, rej: last.rej, f: last.f };
}
export const trend = {
  a: load(KEY, []), cur: [],
  add(p) {
    if (this.cur.length && Math.floor(p.t / 60000) !== Math.floor(this.cur[0].t / 60000)) {
      this.a.push(bucket(this.cur)); this.cur = [];
      if (this.a.length > MAX) this.a.splice(0, this.a.length - MAX);
      try { ls.setItem(KEY, JSON.stringify(this.a)); } catch { this.a.splice(0, 240); }
    }
    this.cur.push(p);
  },
  since(ms, now = Date.now()) { return this.a.filter(p => p.t >= now - ms); },
  clear() { this.a = []; this.cur = []; ls.setItem(KEY, '[]'); },
};
// Efficiency map. Point key = "MHz|mV" (core voltage rounded to 5 mV). Skips the first 3 minutes after a change.
export const lab = { m: load(LAB, {}), lastKey: null, since: 0 };
export function labKey(f, v) { return f && v ? `${f}|${Math.round(v * 1000 / 5) * 5}` : null; }
export function labRecord(p) {
  const k = labKey(p.f, p.mv); if (!k || !(p.gh > 0) || !(p.w > 0)) return;
  if (k !== lab.lastKey) { lab.lastKey = k; lab.since = p.t; return; }
  if (p.t - lab.since < 180e3) return;
  const e = lab.m[k] || (lab.m[k] = { n: 0, gh: 0, w: 0, tmax: 0, last: 0 });
  e.n++; e.gh += p.gh; e.w += p.w; e.tmax = Math.max(e.tmax, p.asic || 0); e.last = p.t;
  if (e.n % 12 === 0) try { ls.setItem(LAB, JSON.stringify(lab.m)); } catch { }
}
export function labPoints() {
  return Object.entries(lab.m).map(([k, e]) => { const [f, mv] = k.split('|').map(Number); const gh = e.gh / e.n, w = e.w / e.n;
    return { f, mv, gh, w, jth: w / (gh / 1000), tmax: e.tmax, mins: e.n * 5 / 60, last: e.last }; }).sort((a, b) => a.f - b.f || a.mv - b.mv);
}
export function labClear() { lab.m = {}; ls.setItem(LAB, '{}'); }
