<script>
  // Frequency / core voltage. Nothing is written until "Review & apply" -> confirm dialog (typed phrase above stock).
  import { app, applySettings } from '../lib/store.svelte.js';
  import { changes, clone } from '../lib/form.js';
  import { isAboveStock } from '../lib/insights.js';
  import { errPct, errLevel, errSummary } from '../lib/errors.js';
  import { BOARD_ERRORS } from '../lib/format.js';
  let e = $state(null), orig = null;
  $effect(() => { if (app.settings && !e) { orig = clone(app.settings); e = clone(app.settings); } });
  let above = $derived(e && isAboveStock(+e.frequency, +e.coreVoltage, app.settings));
  let isSafe = $derived(app.settings && app.settings.frequency <= 700 && app.settings.coreVoltage <= 1150);
  let pending = $derived(e && orig && (+e.frequency !== orig.frequency || +e.coreVoltage !== orig.coreVoltage || +e.vrFrequency !== orig.vrFrequency || +e.jobInterval !== orig.jobInterval));
  const KEYS = ['frequency', 'coreVoltage', 'vrFrequency', 'jobInterval'];
  async function save() {
    const pick = o => Object.fromEntries(KEYS.filter(k => o[k] !== undefined).map(k => [k, +o[k]]));
    const p = changes(pick(orig), pick(e)); if (!p) return;
    if (await applySettings(p, 'Apply ASIC tuning')) e = null;
  }
  async function safe() { if (await applySettings({ frequency: 700, coreVoltage: 1150 }, 'Apply SAFE preset: 700 MHz / 1150 mV')) e = null; }
  const preset = (f, v) => { e.frequency = f; e.coreVoltage = v; };
  let d = $derived(app.dash), p = $derived(d?.performance || {}), pw = $derived(d?.power || {});
  let es = $derived(errSummary(p.chips || []));
  let be = $derived(d?.system?.boardError ? (BOARD_ERRORS[d.system.boardError] || 'error ' + d.system.boardError) : 'none');
</script>
<div class="row"><div><h1>Mining & Tuning</h1><div class="mute">ASIC clock and core voltage. Every change goes through a confirm dialog — no instant slider writes.</div></div></div>
{#if e}
<section class="grid g2">
  <div class="card c-ok safecard" data-testid="safe-card">
    <div class="eyebrow">One-click preset</div>
    <div class="row"><div class="big ok">700 MHz / 1150 mV</div><span class="sp"></span>{#if isSafe}<span class="pill"><span class="dot ok"></span>active now</span>{/if}</div>
    <p class="mute small">The NerdOctAxe safe start. This firmware also lowers any saved higher tuning to 700 / 1150 once on first boot after installing this Costean OS build.</p>
    <button class="safe" onclick={safe} disabled={isSafe} data-testid="safe-btn">Safe 700 MHz / 1150 mV</button>
  </div>
  <div class="card c-cyan"><div class="eyebrow">Live</div>
    <div class="g4 grid" style="margin-top:6px">
      <div class="stat"><div class="lbl">Applied MHz</div><div class="v">{p.frequency ?? '—'}</div></div>
      <div class="stat"><div class="lbl">Core volt</div><div class="v">{pw.coreVoltageActual?.toFixed(3) ?? '—'}<small>V</small></div></div>
      <div class="stat"><div class="lbl">Hashrate</div><div class="v">{((p.hashRate || 0) / 1000).toFixed(2)}<small>TH/s</small></div></div>
      <div class="stat"><div class="lbl">Power</div><div class="v">{pw.watts?.toFixed(0) ?? '—'}<small>W</small></div></div></div>
    <div class="errline" data-testid="live-err"><span class="lbl">ASIC errors</span><b class="{errLevel(es.pct)}">{es.err.toFixed(1)} GH/s · {es.pct.toFixed(2)}%</b><span class="sp"></span><span class="mute small">board error: <b class={be === 'none' ? 'ok' : 'crit'}>{be}</b></span></div></div>
</section>
<div class="card c-blue" data-testid="asic-errors">
  <div class="row"><h2 style="margin:0">ASIC hardware errors · BM1370 ×{(p.chips || []).length || 8}</h2><span class="sp"></span><span class="pill"><span class="dot {errLevel(es.pct)}"></span>total {es.pct.toFixed(2)}%</span></div>
  <div class="ex">{#each p.chips || [] as c, i}{@const ep = errPct(c.hashRate || 0, c.errorHashRate || 0)}
    <div class="ec {errLevel(ep)}"><div class="row"><span class="eyebrow">Chip {i + 1}</span><span class="sp"></span><span class="badge {errLevel(ep)}">{ep.toFixed(2)}%</span></div>
      <div class="num">{(c.hashRate || 0).toFixed(0)} <small class="mute">GH/s valid</small></div>
      <div class="small mute">{(c.errorHashRate || 0).toFixed(1)} GH/s err</div>
      <div class="eb"><i class={errLevel(ep)} style:width="{Math.min(100, ep / 3 * 100)}%"></i></div></div>{/each}</div>
  <div class="mute small" style="margin-top:8px">Green &lt;1% · amber 1–2% · red &gt;2%. If one chip's hardware errors keep rising, the core voltage is usually too low for the chosen MHz.</div>
</div>
<div class="card fm" data-testid="tuning">
  <div class="row"><h2>ASIC clock & voltage</h2><span class="sp"></span>{#if above}<span class="pill crit">above stock — typed confirmation required</span>{:else}<span class="pill"><span class="dot ok"></span>within stock</span>{/if}</div>
  <div class="row">{#if e.ecoFrequency > 0}<button onclick={() => preset(e.ecoFrequency, e.ecoCoreVoltage)}>Eco {e.ecoFrequency}/{e.ecoCoreVoltage}</button>{/if}<button onclick={() => preset(e.defaultFrequency, e.defaultCoreVoltage)}>Stock {e.defaultFrequency}/{e.defaultCoreVoltage}</button><span class="mute small">(fills the form only)</span></div>
  <div class="r2">
    <label class="f"><span>Frequency (MHz) · stock {e.defaultFrequency}</span><select bind:value={e.frequency}>{#each [...new Set([...(e.frequencyOptions || []), +e.frequency])].sort((a, b) => a - b) as f}<option value={f}>{f} MHz</option>{/each}</select></label>
    <label class="f"><span>Core voltage (mV) · limits {e.absMinCoreVoltage}–{e.absMaxCoreVoltage}</span><select bind:value={e.coreVoltage}>{#each [...new Set([...(e.voltageOptions || []), +e.coreVoltage])].sort((a, b) => a - b) as v}<option value={v}>{v} mV</option>{/each}</select></label>
    <label class="f"><span>VR frequency (Hz)</span><input type="number" bind:value={e.vrFrequency} /></label>
    <label class="f"><span>Job interval (ms)</span><input type="number" bind:value={e.jobInterval} /></label>
  </div>
  <div class="row"><span class="mute small">{pending ? 'Unsaved changes — nothing has been sent to the miner yet.' : 'No pending changes.'}</span><span class="sp"></span><button onclick={() => e = null}>Revert</button><button class={above ? 'danger' : 'pri'} onclick={save} disabled={!pending} data-testid="save-tuning">Review & apply</button></div>
</div>
{:else}<div class="card mute">Loading settings…</div>{/if}
<style>.errline{display:flex;gap:10px;align-items:baseline;border-top:1px solid var(--line);margin-top:10px;padding-top:8px}.errline .lbl{font:700 10px var(--ui);letter-spacing:.2em;color:var(--mute);text-transform:uppercase}.errline b{font:800 16px var(--mono)}
.ex{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-top:10px}.ec.ok,.ec.warn,.ec.crit{color:var(--text)}.ec{border:1px solid var(--line2);border-radius:12px;padding:9px 11px;background:#121418;box-shadow:inset 0 2px 0 var(--acc)}.ec.warn{box-shadow:inset 0 2px 0 var(--warn)}.ec.crit{box-shadow:inset 0 2px 0 var(--crit);border-color:var(--crit)}
.ec .num{font-size:20px}.ec .num small{font-weight:600;font-size:11px}.badge{font:700 11px var(--mono);padding:1px 7px;border-radius:999px;border:1px solid currentColor}.badge.ok{color:var(--acc)}
.eb{height:4px;background:#1d2027;border-radius:3px;margin-top:6px;overflow:hidden}.eb i{display:block;height:100%;background:linear-gradient(90deg,var(--acc2),var(--acc))}.eb i.warn{background:var(--warn)}.eb i.crit{background:var(--crit)}
@media (max-width:1100px){.ex{grid-template-columns:repeat(2,1fr)}}.fm{display:grid;gap:12px}.r2{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px}.safecard{display:grid;gap:8px}.safecard button{justify-self:start;padding:10px 18px}</style>
