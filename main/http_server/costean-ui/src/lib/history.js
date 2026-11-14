// Firmware history (/api/v2/dashboard?ts=..) is x100 ints with ms offsets from timestampBase.
export function decodeHistory(h, now = Date.now()) {
  if (!h || !h.timestamps) return [];
  const base = now - (h.timestamps.length ? h.timestamps[h.timestamps.length - 1] : 0);
  return h.timestamps.map((t, i) => ({ t: base + t, hr1m: h.hashrate_1m[i] / 100, hr10m: h.hashrate_10m[i] / 100, hr1h: h.hashrate_1h[i] / 100, asic: h.asicTemp[i] / 100, vr: h.vregTemp[i] / 100 }));
}
/** Local ring buffer for metrics the firmware doesn't keep (power, efficiency). */
export class Ring {
  constructor(max = 4320) { this.max = max; this.a = []; }
  push(p) { this.a.push(p); if (this.a.length > this.max) this.a.splice(0, this.a.length - this.max); }
  since(ms, now = Date.now()) { return this.a.filter(p => p.t >= now - ms); }
}
export const RANGES = [['15m', 9e5], ['1h', 36e5], ['3h', 108e5], ['12h', 432e5], ['24h', 864e5]];
export function downsample(pts, n = 240) { if (pts.length <= n) return pts; const step = pts.length / n; return Array.from({ length: n }, (_, i) => pts[Math.floor(i * step)]); }
