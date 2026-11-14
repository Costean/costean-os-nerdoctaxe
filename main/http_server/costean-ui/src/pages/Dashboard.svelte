<script>
  import CosteanStrip from '../components/CosteanStrip.svelte';
  import { app, ring } from '../lib/store.svelte.js';
  import { fmtHash, fmtDiff, fmtUptime, efficiency, BOARD_ERRORS } from '../lib/format.js';
  import { netDiff, activePool } from '../lib/stats.js';
  import NeonGauge from '../components/NeonGauge.svelte';
  import Chart from '../components/Chart.svelte';
  import { errPct, errLevel, errSummary } from '../lib/errors.js';
  let d = $derived(app.dash); let p = $derived(d?.performance || {}), pw = $derived(d?.power || {}), t = $derived(d?.thermal || {});
  let s = $derived(app.settings || {});
  let expected = $derived(p.frequency && p.smallCoreCount && p.asicCount ? p.frequency * p.smallCoreCount * p.asicCount / 1000 : 0);
  let eff = $derived(efficiency(pw.watts, p.hashRate));
  let pool = $derived(activePool(d));
  let rec = $derived((app.tick, ring.a.slice(-180)));
  let chips = $derived((p.chips || []).map((c, i) => ({ gh: c.hashRate || 0, err: c.errorHashRate || 0, ep: errPct(c.hashRate || 0, c.errorHashRate || 0), t: t.asicTemps?.[i] })));
  let es = $derived(errSummary(p.chips || []));
  let avg = $derived(chips.length ? chips.reduce((a, c) => a + c.gh, 0) / chips.length : 0);
  const tc = v => v == null ? 'var(--mute)' : v >= 70 ? 'var(--crit)' : v >= 63 ? 'var(--warn)' : v >= 55 ? 'var(--ice)' : 'var(--acc)';
  const vc = v => v == null ? 'var(--mute)' : v >= 90 ? 'var(--crit)' : v >= 80 ? 'var(--warn)' : 'var(--acc2)';
  let status = $derived(!d ? '' : d.system?.shutdown ? 'ASIC SHUTDOWN' : (t.vrTemp >= 85) ? 'VR HOT — CHECK AIRFLOW' : (d.system?.uptime < 300) ? 'RAMPING UP' : (expected && p.hashRate >= 0.94 * expected) ? 'AT TARGET' : 'BELOW TARGET');
  let rr = $derived((p.sharesAccepted || 0) + (p.sharesRejected || 0) ? (p.sharesRejected || 0) / ((p.sharesAccepted || 0) + (p.sharesRejected || 0)) * 100 : 0);
</script>
{#if d}
{#if d.system?.boardError}<div class="card crit">⚠ {BOARD_ERRORS[d.system.boardError] || 'Board error ' + d.system.boardError}</div>{/if}
<section class="hero">
  <div class="card gauge" data-testid="gauge">
    <div class="eyebrow">Measured hashrate</div>
    <NeonGauge value={p.hashRate || 0} max={Math.max(expected * 1.15, p.hashRate || 1)} text={((p.hashRate || 0) / 1000).toFixed(2)} unit="TH/s" sub={'UP ' + fmtUptime(d.system?.uptime).toUpperCase()} />
    <div class="row status"><span class="dot {status === 'AT TARGET' ? 'ok' : 'warn'}"></span><b class={status === 'AT TARGET' ? 'ok' : 'warn'}>{status}</b><span class="sp"></span><span class="mute small">{expected ? ((p.hashRate || 0) / expected * 100).toFixed(1) + '% of ' + (expected / 1000).toFixed(2) + ' TH/s core math' : ''}</span></div>
  </div>
  <div class="vit">
    <div class="card stat"><div class="lbl">PKG max</div><div class="v" style:color={tc(t.asicTemp)}>{t.asicTemp?.toFixed(1) ?? '—'}<small>°C</small></div></div>
    <div class="card stat c-blue"><div class="lbl">VR board</div><div class="v" style:color={vc(t.vrTemp)}>{t.vrTemp?.toFixed(1) ?? '—'}<small>°C</small></div></div>
    <div class="card stat c-ice"><div class="lbl">VR int</div><div class="v" style:color={vc(t.vrTempInt)}>{t.vrTempInt ? t.vrTempInt.toFixed(1) : '—'}<small>°C</small></div></div>
    <div class="card stat c-blue"><div class="lbl">Power</div><div class="v" style:color="var(--acc2)">{pw.watts?.toFixed(1) ?? '—'}<small>W</small></div><div class="mute small">{eff ? eff.toFixed(2) + ' J/TH' : ''} · {pw.voltage?.toFixed(2)} V in</div></div>
    <div class="card stat"><div class="lbl">Core volt</div><div class="v">{pw.coreVoltageActual?.toFixed(3) ?? '—'}<small>V</small></div><div class="mute small">requested {(s.coreVoltage / 1000 || 0).toFixed(3)} V</div></div>
    <div class="card stat c-cyan"><div class="lbl">Applied / requested MHz</div><div class="v">{p.frequency ?? '—'}<small>/ {s.frequency ?? '—'}</small></div><div class="mute small">fan {t.fans?.[0]?.rpm ?? '—'} rpm · {t.fans?.[0]?.speed ?? '—'}%</div></div>
  </div>
</section>
<CosteanStrip />
<section class="card c-cyan" data-testid="matrix">
  <div class="row"><h2>8-chip matrix · BM1370 ×{p.asicCount || chips.length}</h2><span class="sp"></span><span class="mute small">avg {avg.toFixed(0)} GH/s per chip · </span><span class="small {errLevel(es.pct)}" data-testid="matrix-err">errors {es.err.toFixed(1)} GH/s · {es.pct.toFixed(2)}%</span></div>
  <div class="mx">{#each chips as c, i}
    <div class="chip" style:--c={tc(c.t)} class:weak={avg && c.gh < 0.9 * avg}>
      <div class="row"><span class="eyebrow">Chip {i + 1}</span><span class="sp"></span>{#if avg && c.gh < 0.9 * avg}<span class="warn small">weak</span>{/if}{#if c.ep > 2}<span class="crit small">&nbsp;high err</span>{/if}</div>
      <div class="gh">{c.gh.toFixed(0)}<small>GH/s</small></div>
      <div class="row tl"><span class="tt" style:color={tc(c.t)}>{c.t?.toFixed(1) ?? '—'} °C</span><span class="sp"></span><span class="er {errLevel(c.ep)}">{c.err.toFixed(1)} GH/s · {c.ep.toFixed(2)}% err</span></div>
      <div class="bar"><i style:width="{Math.min(100, avg ? c.gh / (avg * 1.2) * 100 : 0)}%"></i></div>
    </div>{/each}</div>
</section>
<section class="grid g2">
  <div class="card c-ice"><h2>Hash trend</h2><Chart height={190} unit="GH/s" series={[{ name: 'Hashrate', color: '#d9733a', points: rec.map(x => ({ t: x.t, v: x.gh })) }]} /></div>
  <div class="card c-blue"><h2>Temperatures</h2><Chart height={190} unit="°C" series={[{ name: 'PKG max', color: '#f59e0b', points: rec.map(x => ({ t: x.t, v: x.asic })) }, { name: 'VR board', color: '#3fd8c2', points: rec.map(x => ({ t: x.t, v: x.vr })) }, { name: 'VR int', color: '#b48cf0', points: rec.map(x => ({ t: x.t, v: x.vri })) }]} /></div>
</section>
<section class="strip card c-ok">
  <div><span class="eyebrow">Best</span><b class="num" style:color="var(--eblue)">{fmtDiff(p.bestDiff)}</b></div>
  <div><span class="eyebrow">A / R</span><b class="num"><span class="ok">{p.sharesAccepted ?? 0}</span> / {p.sharesRejected ?? 0}</b><span class="mute small">{rr.toFixed(2)}% rej</span></div>
  <div><span class="eyebrow">Pool</span><b class="mono small">{pool?.host || '—'}:{pool?.port || ''}</b><span class="mute small">{pool?.connected ? 'connected' : 'offline'}{pool?.pingRtt ? ' · ' + pool.pingRtt.toFixed(0) + ' ms' : ''}</span></div>
  <div><span class="eyebrow">IP address</span><b class="num">{app.system?.network?.ipAddr || '—'}</b></div>
</section>
{:else}<div class="card mute">Connecting to miner…</div>{/if}
<style>
.hero{display:grid;grid-template-columns:minmax(300px,380px) 1fr;gap:14px}.gauge{display:grid;gap:6px}.status{border-top:1px solid var(--line);padding-top:10px}
.vit{display:grid;grid-template-columns:repeat(3,1fr);gap:12px}
.mx{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-top:6px}
.chip{border:1px solid var(--line2);border-radius:12px;padding:10px 12px;background:#121418;box-shadow:inset 0 2px 0 var(--c)}.chip.weak{border-color:var(--warn)}
.gh{font:800 24px var(--mono);letter-spacing:-.02em}.gh small{font:600 11px var(--ui);color:var(--mute);margin-left:4px}.tt{font:700 15px var(--mono)}.tl{gap:6px;flex-wrap:nowrap}.er{font:600 11px var(--mono);white-space:nowrap}.er.ok{color:var(--acc)}
.bar{height:4px;background:#1d2027;border-radius:3px;margin-top:6px;overflow:hidden}.bar i{display:block;height:100%;background:linear-gradient(90deg,var(--eblue),var(--acc))}
.strip{display:grid;grid-template-columns:repeat(4,1fr);gap:12px}.strip>div{display:grid;gap:2px}.strip b{font-size:20px}
@media (max-width:1100px){.hero{grid-template-columns:1fr}.vit{grid-template-columns:repeat(2,1fr)}.mx{grid-template-columns:repeat(2,1fr)}.strip{grid-template-columns:repeat(2,1fr)}}
</style>
