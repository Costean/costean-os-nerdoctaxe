// Mining math. All pure functions, unit-tested.
export const TWO32 = 4294967296;
/** expected seconds to find a hash meeting difficulty d at gh GH/s */
export const expectSecs = (d, gh) => (d > 0 && gh > 0) ? d * TWO32 / (gh * 1e9) : Infinity;
/** probability of at least one hit within T seconds */
export const probWithin = (d, gh, T) => { const e = expectSecs(d, gh); return isFinite(e) ? 1 - Math.exp(-T / e) : 0; };
export const fmtDur = s => { if (!isFinite(s)) return '∞'; const u = [['yr', 31557600], ['d', 86400], ['h', 3600], ['min', 60], ['s', 1]];
  for (const [n, v] of u) if (s >= v) { const x = s / v; return (x >= 1e6 ? x.toExponential(1) : x >= 100 ? Math.round(x).toLocaleString() : x.toFixed(1)) + ' ' + n; } return s.toFixed(1) + ' s'; };
export const fmtOdds = p => !(p > 0) ? '—' : p >= 0.01 ? (p * 100).toFixed(2) + '%' : '1 in ' + Math.round(1 / p).toLocaleString();
/** network difficulty from the dashboard payload */
export function netDiff(d) {
  const ps = d?.stratum?.pools || []; for (const p of ps) if (p.networkDifficulty > 0) return p.networkDifficulty;
  for (const b of d?.coinbase?.blockHeaders || []) if (b.networkDifficulty > 0) return b.networkDifficulty;
  return 0;
}
export const activePool = d => (d?.stratum?.pools || []).find(p => p.connected) || null;
/** share luck: accepted shares in window vs expected at pool difficulty. >1 = lucky. */
export function shareLuck(acc0, acc1, secs, gh, poolDiff) {
  if (!(secs > 0) || !(poolDiff > 0) || !(gh > 0) || acc1 == null || acc0 == null) return null;
  const expected = secs / expectSecs(poolDiff, gh); return expected > 0 ? { got: acc1 - acc0, expected, luck: (acc1 - acc0) / expected } : null;
}
/** where best diff sits on the way to a block: log10 progress 0..1 */
export const ladder = (best, net) => (best > 1 && net > 1) ? Math.min(1, Math.log10(best) / Math.log10(net)) : 0;
/** simple stats */
export function describe(v) {
  const a = v.filter(x => x != null && !isNaN(x)).sort((x, y) => x - y); if (!a.length) return null;
  const q = f => a[Math.min(a.length - 1, Math.floor(f * a.length))]; const mean = a.reduce((s, x) => s + x, 0) / a.length;
  const sd = Math.sqrt(a.reduce((s, x) => s + (x - mean) ** 2, 0) / a.length);
  return { n: a.length, min: a[0], max: a[a.length - 1], mean, sd, p5: q(.05), p50: q(.5), p95: q(.95), cv: mean ? sd / mean : 0 };
}
export function histogram(v, bins = 24) {
  const s = describe(v); if (!s) return []; const lo = s.p5, hi = s.p95 > lo ? s.p95 : lo + 1, w = (hi - lo) / bins;
  const h = Array.from({ length: bins }, (_, i) => ({ x0: lo + i * w, x1: lo + (i + 1) * w, n: 0 }));
  for (const x of v) if (x != null && !isNaN(x)) h[Math.max(0, Math.min(bins - 1, Math.floor((x - lo) / w)))].n++;
  return h;
}
/** chip "fingerprint": per chip share of total hashrate & deviation */
export function chipBalance(chr) { const t = chr.reduce((a, b) => a + b, 0), m = t / (chr.length || 1); return chr.map(h => ({ share: t ? h / t : 0, dev: m ? (h - m) / m : 0 })); }
/** daily energy and cost */
export const energy = (w, price) => ({ kwhDay: w * 24 / 1000, costDay: w * 24 / 1000 * (price || 0) });
