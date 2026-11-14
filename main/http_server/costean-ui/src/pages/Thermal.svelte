<script>
  // Fans / PID / overheat + VR headroom monitor. Air-cooling friendly. Writes only via confirm dialog.
  import { app, applySettings, ring } from '../lib/store.svelte.js';
  import { changes, clone } from '../lib/form.js';
  import Chart from '../components/Chart.svelte';
  let e = $state(null), orig = null;
  $effect(() => { if (app.settings && !e) { orig = clone(app.settings); e = clone(app.settings); } });
  const KEYS = ['fans', 'invertFanPolarity', 'pidUseMax', 'flipScreen', 'invertScreen', 'autoScreenOff'];
  async function save() {
    const pick = o => Object.fromEntries(KEYS.filter(k => o[k] !== undefined).map(k => [k, o[k]]));
    const a = pick(orig), b = pick(e);
    for (const k of ['invertFanPolarity', 'flipScreen', 'invertScreen', 'autoScreenOff']) { if (k in b) b[k] = !!b[k]; if (k in a) a[k] = !!a[k]; }
    const p = changes(a, b); if (!p) return;
    if (await applySettings(p, 'Save thermal & fan settings')) e = null;
  }
  let t = $derived(app.dash?.thermal || {}), sys = $derived(app.dash?.system || {});
  let rec = $derived((app.tick, ring.a.slice(-180)));
  const VR_LIMIT = 85;   // display guide only (firmware protection thresholds are unchanged)
  const head = v => v == null ? null : Math.max(0, VR_LIMIT - v);
  let pidT = $derived(app.settings?.fans?.[0]?.pid?.targetTemp);
</script>
<h1>Thermal & Safe Mode</h1><div class="mute">Fan PID, overheat protection and regulator headroom. Tuned for air cooling (stock fan + heatsink).</div>
<section class="grid g3">
  {#each [['PKG max', t.asicTemp, pidT ? `PID target ${pidT} °C` : '', 'c-ice'], ['VR board', t.vrTemp, `${head(t.vrTemp)?.toFixed(1) ?? '—'} °C headroom to ${VR_LIMIT} °C`, 'c-blue'], ['VR int', t.vrTempInt, `${head(t.vrTempInt)?.toFixed(1) ?? '—'} °C headroom`, 'c-cyan']] as [l, v, sub, c]}
  <div class="card stat {c}"><div class="lbl">{l}</div><div class="v">{v ? v.toFixed(1) : '—'}<small>°C</small></div><div class="hb"><i style:width="{Math.min(100, (v || 0) / VR_LIMIT * 100)}%" class:hot={v >= 78}></i></div><div class="mute small">{sub}</div></div>
  {/each}
</section>
<div class="card c-blue" data-testid="vr-monitor"><div class="row"><h2>VR headroom monitor</h2><span class="sp"></span>
  {#if (t.vrTemp ?? 0) < 78}<span class="pill"><span class="dot ok"></span>HEADROOM OK</span>{:else}<span class="pill"><span class="dot warn"></span>WAITING: VR BOARD HEADROOM</span>{/if}</div>
  <Chart height={180} unit="°C" series={[{ name: 'VR board', color: '#3fd8c2', points: rec.map(x => ({ t: x.t, v: x.vr })) }, { name: 'VR int', color: '#b48cf0', points: rec.map(x => ({ t: x.t, v: x.vri })) }, { name: 'PKG max', color: '#f59e0b', points: rec.map(x => ({ t: x.t, v: x.asic })) }]} />
  <div class="mute small">Firmware overheat shutdown: {sys.overheatTemp ?? '—'} °C (unchanged stock protection).</div></div>
{#if e}
<div class="grid g2">
  <div class="card fm c-cyan"><h2>Fans</h2>
    {#each e.fans || [] as f, i}
      <div class="fan"><b>{f.label || 'Fan ' + (i + 1)}</b>
        <div class="r2"><label class="f"><span>Mode</span><select bind:value={f.mode}>{#if i > 0}<option value={3}>Use fan 1 settings</option>{/if}<option value={0}>Manual</option>{#if f.pid?.targetTemp !== -1}<option value={2}>Auto (PID)</option>{/if}</select></label>
          {#if f.mode === 0}<label class="f"><span>Speed {f.manualSpeed}%</span><input type="range" min="0" max="100" bind:value={f.manualSpeed} /></label>{/if}
          {#if f.mode === 2}<label class="f"><span>PID target °C</span><input type="number" bind:value={f.pid.targetTemp} /></label>{/if}
          <label class="f"><span>Overheat shutdown °C</span><input type="number" bind:value={f.overheatTemp} /></label></div>
        {#if f.mode === 2}<details><summary class="mute small">PID gains</summary><div class="r2"><label class="f"><span>P</span><input type="number" step="0.01" bind:value={f.pid.p} /></label><label class="f"><span>I</span><input type="number" step="0.01" bind:value={f.pid.i} /></label><label class="f"><span>D</span><input type="number" step="0.01" bind:value={f.pid.d} /></label></div></details>{/if}
      </div>
    {/each}
    <label class="chk"><input type="checkbox" checked={!!e.invertFanPolarity} onchange={ev => e.invertFanPolarity = ev.currentTarget.checked} /> Invert fan polarity</label>
    {#if e.pidUseMax !== undefined}<label class="chk"><input type="checkbox" bind:checked={e.pidUseMax} /> PID follows hottest chip</label>{/if}
  </div>
  <div class="card fm c-ice"><h2>LCD</h2>
    <p class="mute small">The 1.9" screen cycles 4 pages with the button: 1/4 hashrate · 2/4 regulator · 3/4 octa matrix · 4/4 blocks & network.</p>
    <label class="chk"><input type="checkbox" checked={!!e.flipScreen} onchange={ev => e.flipScreen = ev.currentTarget.checked} /> Flip screen</label>
    <label class="chk"><input type="checkbox" checked={!!e.invertScreen} onchange={ev => e.invertScreen = ev.currentTarget.checked} /> Invert colours</label>
    <label class="chk"><input type="checkbox" checked={!!e.autoScreenOff} onchange={ev => e.autoScreenOff = ev.currentTarget.checked} /> Auto screen off</label></div>
</div>
<div class="row"><span class="sp"></span><button onclick={() => e = null}>Revert</button><button class="pri" onclick={save} data-testid="save-thermal">Review & save</button></div>
{/if}
<style>.fm{display:grid;gap:10px;align-content:start}.r2{display:flex;gap:10px;flex-wrap:wrap;align-items:end}.r2>*{flex:1;min-width:120px}.fan{display:grid;gap:6px;padding:10px;border:1px solid var(--line2);border-radius:12px}
.hb{height:5px;background:#1d2027;border-radius:3px;margin:8px 0 4px;overflow:hidden}.hb i{display:block;height:100%;background:linear-gradient(90deg,var(--acc),var(--acc2))}.hb i.hot{background:linear-gradient(90deg,var(--warn),#f59e0b)}</style>
