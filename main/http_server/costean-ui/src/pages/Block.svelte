<script>
  // Solo-mining showcase: best share vs network target, block candidate status, difficulty observatory (log scale).
  import { onMount } from 'svelte';
  import { app, ring } from '../lib/store.svelte.js';
  import { api } from '../lib/api.js';
  let info = $state(null);
  onMount(() => { const f = () => api.info().then(v => info = v).catch(() => {}); f(); const iv = setInterval(f, 15000); return () => clearInterval(iv); });
  import { fmtDiff } from '../lib/format.js';
  import { netDiff, expectSecs, probWithin, fmtDur, fmtOdds } from '../lib/stats.js';
  let d = $derived(app.dash), p = $derived(d?.performance || {});
  let nd = $derived(netDiff(d));
  let bh = $derived(d?.coinbase?.blockHeaders?.[0]);
  let found = $derived((info?.foundBlocks || 0) > 0 || (nd > 0 && p.bestDiff >= nd));
  let pct = $derived(nd > 0 && p.bestDiff > 0 ? Math.min(100, Math.log10(p.bestDiff) / Math.log10(nd) * 100) : 0);
  let rec = $derived((app.tick, ring.a.slice(-240).filter(x => x.bsd || x.pd)));
  let peak = $derived(Math.max(0, ...rec.map(x => x.bsd || 0)));
  let latest = $derived(rec.length ? rec[rec.length - 1].bsd : p.bestSessionDiff);
  const W = 900, H = 260, L = 64, R = 14, T = 16, B = 26;
  let lo = $derived(Math.max(1, Math.min(...rec.map(x => x.pd || x.bsd || 1)) / 3));
  let hi = $derived(Math.max(nd || 1, peak) * 2.5);
  const yy = v => T + (1 - (Math.log10(Math.max(v, lo)) - Math.log10(lo)) / ((Math.log10(hi) - Math.log10(lo)) || 1)) * (H - T - B);
  const xx = i => L + (rec.length > 1 ? i / (rec.length - 1) : 1) * (W - L - R);
  const path = k => rec.map((x, i) => x[k] ? `${i ? 'L' : 'M'}${xx(i).toFixed(1)},${yy(x[k]).toFixed(1)}` : '').join('');
  let ticks = $derived(Array.from({ length: Math.ceil(Math.log10(hi)) - Math.floor(Math.log10(lo)) + 1 }, (_, i) => 10 ** (Math.floor(Math.log10(lo)) + i)).filter(v => v >= lo && v <= hi).filter((_, i, a) => a.length < 8 || i % 2 === 0));
  let gh = $derived(p.hashRate || 0);
</script>
{#if d}
<section class="card c-gold hero" data-testid="block-card">
  <div class="badge" class:lit={found}>{found ? '★' : '₿'}</div>
  <div class="mid">
    <div class="eyebrow">Every hash is a new chance</div>
    <div class="ht">{found ? 'BLOCK CANDIDATE' : 'HUNTING FOR A BLOCK'}</div>
    <div class="mute">{found ? 'A share met the network target. Check your pool / wallet — this is not a payment confirmation.' : 'Solo lottery: a block is found when one share reaches the network difficulty.'}</div>
    <div class="row cmp">
      <div><span class="eyebrow">Best share (all time)</span><b class="num eblue">{fmtDiff(p.bestDiff)}</b></div>
      <div class="vs">vs</div>
      <div><span class="eyebrow">Network difficulty</span><b class="num blue">{nd ? fmtDiff(nd) : '—'}</b></div>
    </div>
    <div class="track"><i style:width="{pct}%"></i><span class="tgt"></span></div>
    <div class="row small mute"><span>{pct.toFixed(1)}% of the way (log scale)</span><span class="sp"></span><span>NETWORK TARGET</span></div>
  </div>
  <div class="facts">
    <div><span class="eyebrow">Coin</span><b>Bitcoin</b></div>
    <div><span class="eyebrow">Block height (job)</span><b class="num">{bh?.blockHeight ?? '—'}</b></div>
    <div><span class="eyebrow">Coinbase output (job)</span><b class="num">{bh?.coinbaseValueTotalSatoshis ? (bh.coinbaseValueTotalSatoshis / 1e8).toFixed(8) + ' BTC' : '—'}</b></div>
    <div><span class="eyebrow">Blocks found</span><b class="num gold">{info?.foundBlocks ?? '—'}</b></div>
  </div>
</section>
<section class="card c-ice" data-testid="observatory">
  <div class="row"><div><h2 style="margin:0">Difficulty observatory</h2><div class="mute small">Session best-share samples and pool share target against the network target (log scale). Recorded by this browser tab.</div></div></div>
  <div class="grid g3 mini">
    <div class="card stat c-cyan"><div class="lbl">Latest session best</div><div class="v">{fmtDiff(latest)}</div></div>
    <div class="card stat"><div class="lbl">Observed peak</div><div class="v eblue">{fmtDiff(peak || p.bestSessionDiff)}</div></div>
    <div class="card stat c-blue"><div class="lbl">Network difficulty</div><div class="v blue">{nd ? fmtDiff(nd) : '—'}</div></div>
  </div>
  {#if rec.length > 1}
  <svg viewBox="0 0 {W} {H}" class="obs" role="img" aria-label="difficulty chart">
    {#each ticks as v}<line x1={L} x2={W - R} y1={yy(v)} y2={yy(v)} class="gl"/><text x={L - 8} y={yy(v) + 4} text-anchor="end" class="ax">{fmtDiff(v)}</text>{/each}
    {#if nd}<line x1={L} x2={W - R} y1={yy(nd)} y2={yy(nd)} class="net"/><text x={L + 6} y={yy(nd) - 6} class="nt">NETWORK TARGET {fmtDiff(nd)}</text>{/if}
    {#if peak}<line x1={L} x2={W - R} y1={yy(peak)} y2={yy(peak)} class="pk"/>{/if}
    <path d={path('pd')} class="pd"/>
    <path d={path('bsd')} class="bs"/>
  </svg>
  <div class="legend small"><span><i style="background:#fff"></i>Session best</span><span><i style="background:#ee8a4c"></i>Pool share target</span><span><i style="background:#f6cb6e"></i>Network target</span><span><i style="background:#d9733a"></i>Session peak</span></div>
  {:else}<div class="mute empty">Collecting samples… (one every 5 s)</div>{/if}
</section>
<section class="grid g3">
  <div class="card stat c-cyan"><div class="lbl">Expected time to block</div><div class="v">{fmtDur(expectSecs(nd, gh))}</div></div>
  <div class="card stat c-ice"><div class="lbl">Chance today</div><div class="v">{fmtOdds(probWithin(nd, gh, 86400))}</div></div>
  <div class="card stat c-ok"><div class="lbl">Chance this year</div><div class="v">{fmtOdds(probWithin(nd, gh, 31557600))}</div></div>
</section>
{:else}<div class="card mute">Connecting to miner…</div>{/if}
<style>
.hero{display:grid;grid-template-columns:110px 1fr 260px;gap:22px;align-items:center;padding:22px;background:radial-gradient(600px 240px at 20% 0%,#3fd8c233,transparent),linear-gradient(180deg,#1a1d23,#121418)}
.badge{width:96px;height:96px;border-radius:50%;display:grid;place-items:center;font:800 40px var(--ui);color:#fff;background:radial-gradient(circle at 35% 30%,#e8a070,#3a3f49);border:2px solid #f0b48a;box-shadow:0 0 30px -4px #ee8a4c}
.badge.lit{background:radial-gradient(circle at 35% 30%,#ffe9a0,#c98a12);border-color:var(--gold);box-shadow:0 0 40px -2px var(--gold);color:#3a2500}
.mid{display:grid;gap:8px}.ht{font:900 34px var(--ui);letter-spacing:.02em;text-shadow:0 0 18px #f0b48a88}
.cmp{gap:18px}.cmp>div{display:grid}.cmp b{font-size:30px}.vs{color:var(--mute);font-weight:700}
.eblue{color:var(--eblue)}.blue{color:var(--acc2)}.gold{color:var(--gold)}
.track{position:relative;height:10px;background:#0e1013;border:1px solid var(--line2);border-radius:6px}.track i{display:block;height:100%;border-radius:6px;background:linear-gradient(90deg,var(--eblue),var(--ice),var(--acc))}.tgt{position:absolute;right:-1px;top:-5px;width:3px;height:18px;background:var(--gold);box-shadow:0 0 8px var(--gold)}
.facts{display:grid;gap:10px;border-left:1px solid var(--line);padding-left:18px}.facts>div{display:grid}.facts b{font-size:16px}
.mini{margin:12px 0}.obs{width:100%;height:auto;display:block}.gl{stroke:#2a2e36}.ax{fill:var(--mute);font-size:11px;font-family:var(--mono)}
.net{stroke:#f6cb6e;stroke-dasharray:6 5;stroke-width:1.6}.nt{fill:#f6cb6e;font:700 11px var(--ui);letter-spacing:.12em}.pk{stroke:#d9733a;stroke-dasharray:2 4}
.bs{fill:none;stroke:#fff;stroke-width:2;filter:drop-shadow(0 0 4px #7fe6d6)}.pd{fill:none;stroke:#ee8a4c;stroke-width:1.5;opacity:.8}
.legend{display:flex;gap:14px;color:var(--mute);margin-top:6px}.legend i{display:inline-block;width:12px;height:3px;margin-right:6px;vertical-align:middle;border-radius:2px}.empty{padding:40px;text-align:center}
@media (max-width:1100px){.hero{grid-template-columns:1fr}.facts{border:0;padding:0;grid-template-columns:1fr 1fr}}
</style>
